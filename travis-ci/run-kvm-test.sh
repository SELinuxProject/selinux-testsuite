#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

# Based on SELinux userspace CI scripts from:
# https://github.com/SELinuxProject/selinux

set -ex

TEST_RUNNER="$1"

if [ -z "$TEST_RUNNER" ]; then
    echo "$0: expected script to be run on the command line!" 1>&2
    exit 1
fi

#
# Variables for controlling the Fedora Image version and download URLs.
#
if [ -z "$FEDORA_KIND" ] || [ -z "$FEDORA_MAJOR" ]; then
    echo "$0: FEDORA_KIND and FEDORA_MAJOR must be set!" 1>&2
    exit 1
fi

BASE_URL="https://download.fedoraproject.org/pub/fedora/linux/$FEDORA_KIND/$FEDORA_MAJOR/Cloud/x86_64/images"
GPG_URL="https://getfedora.org/static/fedora.gpg"

#
# Travis gives us 7.5GB of RAM and two cores:
# https://docs.travis-ci.com/user/reference/overview/
#
MEMORY=4096
VCPUS="$(nproc)"

#
# Get the Fedora Cloud Image, It is a base image that small and ready to go, extract it and modify it with virt-sysprep
#  - https://alt.fedoraproject.org/en/verify.html
cd "$HOME"
wget -r -nd -np -l 1 -H -e robots=off -A "*.raw.xz,*-CHECKSUM" "$BASE_URL"
if [ $(ls -1q *.raw.xz | wc -l) -ne 1 ]; then
    echo "$0: too many image files downloaded!" 1>&2
    exit 1
fi

# Verify the image (skip GPG for unsigned rawhide images)
if [ "$FEDORA_KIND" != "development" ]; then
    curl "$GPG_URL" | gpg --import
    gpg --verify-files ./*-CHECKSUM
fi
sha256sum --ignore-missing -c ./*-CHECKSUM

# Extract the image
unxz -T0 *.raw.xz

# Search is needed for $HOME so virt service can access the image file.
chmod a+x "$HOME"

#
# Modify the virtual image to:
#   - Enable a login, we just use root
#   - Enable passwordless login
#     - Force a relabel to fix labels on ssh keys
#
sudo virt-sysprep -a *.raw \
  --root-password password:123456 \
  --hostname fedoravm \
  --append-line '/etc/ssh/sshd_config:PermitRootLogin yes' \
  --append-line '/etc/ssh/sshd_config:PubkeyAuthentication yes' \
  --mkdir /root/.ssh \
  --upload "$HOME/.ssh/id_rsa.pub:/root/.ssh/authorized_keys" \
  --chmod '0600:/root/.ssh/authorized_keys' \
  --run-command 'chown root:root /root/.ssh/authorized_keys' \
  --copy-in "$TRAVIS_BUILD_DIR:/root" \
  --network \
  --selinux-relabel

#
# Now we create a domain by using virt-install. This not only creates the domain, but runs the VM as well
# It should be ready to go for ssh, once ssh starts.
#
sudo virt-install \
  --name fedoravm \
  --memory $MEMORY \
  --vcpus $VCPUS \
  --disk *.raw \
  --import --noautoconsole

#
# Here comes the tricky part, we have to figure out when the VM comes up AND we need the ip address for ssh. So we
# can check the net-dhcp leases, for our host. We have to poll, and we will poll for up to 3 minutes in 6 second
# intervals, so 30 poll attempts (0-29 inclusive).
#
# We have a full reboot + relabel, so first sleep gets us close
#
sleep 30
for i in $(seq 0 29); do
    echo "loop $i"
    sleep 6s
    # Get the leases, but tee it so it's easier to debug
    sudo virsh net-dhcp-leases default | tee dhcp-leases.txt

    # get our ipaddress
    ipaddy="$(grep fedoravm dhcp-leases.txt | awk '{print $5}' | cut -d'/' -f 1-1)"
    if [ -n "$ipaddy" ]; then
        # found it, we're done looking, print it for debug logs
        echo "ipaddy: $ipaddy"
        break
    fi
    # it's empty/not found, loop back and try again.
done

# Did we find it? If not die.
if [ -z "$ipaddy" ]; then
    echo "$0: ipaddy zero length, exiting with error 1" 1>&2
    exit 1
fi

#
# Great we have a host running, ssh into it. We specify -o so
# we don't get blocked on asking to add the servers key to
# our known_hosts. Also, we need to forward the project directory
# so forks know where to go.
#

# First update to the latest kernel.
ssh -tt -o StrictHostKeyChecking=no -o LogLevel=QUIET "root@$ipaddy" \
    dnf install -y kernel

# Then reboot.
sudo virsh reboot fedoravm
sleep 5

while ! nc -w 10 -z "$ipaddy" 22; do sleep 0.5s; done

# And run the testsuite.
project_dir="$(basename "$TRAVIS_BUILD_DIR")"
ssh -tt -o StrictHostKeyChecking=no -o LogLevel=QUIET "root@$ipaddy" "SELINUX_DIR=/root/$project_dir /root/$project_dir/$TEST_RUNNER"

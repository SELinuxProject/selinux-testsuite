#!/bin/bash

set -ex

if ! [ -d selinux-policy/.git ]; then
	git clone --recursive https://github.com/fedora-selinux/selinux-policy
else
	git -C selinux-policy fetch origin
	git -C selinux-policy/policy/modules/contrib fetch origin
fi
git -C selinux-policy checkout origin/rawhide
git -C selinux-policy/policy/modules/contrib checkout origin/rawhide

if ! [ -d container-selinux/.git ]; then
	git clone https://github.com/containers/container-selinux.git
	for f in container.if container.te; do
		ln -s ../../../../container-selinux/$f \
			selinux-policy/policy/modules/contrib/$f
	done
else
	git -C container-selinux fetch origin
fi
git -C container-selinux checkout origin/master

cd selinux-policy

grep -q refpolicy build.conf && sed -i 's/refpolicy/targeted/' build.conf

make -j`nproc --all` BINDIR=/usr/local/bin SBINDIR=/usr/local/sbin
sudo make install install-headers

# workaround for different Makefile location in Fedora RPMs
sudo ln -s include/Makefile /usr/share/selinux/targeted/Makefile

#!/bin/bash

set -ex

if ! [ -d selinux-policy/.git ]; then
	git clone --recursive https://github.com/fedora-selinux/selinux-policy
	(cd selinux-policy/policy/modules/contrib && git checkout rawhide)
else
	(cd selinux-policy && git pull || { git checkout '*' && git pull; })
	(cd selinux-policy/policy/modules/contrib && git pull)
fi

if ! [ -d container-selinux/.git ]; then
	git clone https://github.com/containers/container-selinux.git
	for f in container.if container.te; do
		ln -s ../../../../container-selinux/$f \
			selinux-policy/policy/modules/contrib/$f
	done
else
	(cd container-selinux && git pull)
fi

cd selinux-policy

grep -q refpolicy build.conf && sed -i 's/refpolicy/targeted/' build.conf

[ -f policy/modules.conf ] || make conf

make -j`nproc --all` BINDIR=/usr/local/bin SBINDIR=/usr/local/sbin
sudo make install install-headers

# workaround for different Makefile location in Fedora RPMs
sudo ln -s include/Makefile /usr/share/selinux/targeted/Makefile

#!/bin/bash

set -ex

if ! [ -d refpolicy/.git ]; then
	git clone https://github.com/SELinuxProject/refpolicy
else
	git pull || { git checkout '*' && git pull; }
fi

cd refpolicy

[ -f policy/modules.conf ] || make conf

grep -q '^portcon sctp' policy/modules/kernel/corenetwork.te.in && \
	sed -i '/^portcon sctp/d' policy/modules/kernel/corenetwork.te.in

make -j`nproc --all`
sudo make install install-headers

# workaround for different Makefile location in Fedora RPMs
sudo ln -s include/Makefile /usr/share/selinux/refpolicy/Makefile

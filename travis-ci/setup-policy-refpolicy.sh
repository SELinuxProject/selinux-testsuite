#!/bin/bash

set -ex

if ! [ -d refpolicy/.git ]; then
	git clone https://github.com/SELinuxProject/refpolicy
else
	git -C refpolicy fetch origin
fi

cd refpolicy

git checkout origin/master

make conf

make -j`nproc --all` BINDIR=/usr/local/bin SBINDIR=/usr/local/sbin
sudo make install install-headers

# workaround for different Makefile location in Fedora RPMs
sudo ln -s include/Makefile /usr/share/selinux/refpolicy/Makefile

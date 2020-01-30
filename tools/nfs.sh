#!/bin/sh -e
MOUNT=`stat --print %m .`
TESTDIR=`pwd`
systemctl start nfs-server
exportfs -orw,no_root_squash,security_label localhost:$MOUNT
mkdir -p /mnt/selinux-testsuite
mount -t nfs -o vers=4.2 localhost:$TESTDIR /mnt/selinux-testsuite
pushd /mnt/selinux-testsuite
make test
popd
umount /mnt/selinux-testsuite
exportfs -u localhost:$MOUNT
systemctl stop nfs-server

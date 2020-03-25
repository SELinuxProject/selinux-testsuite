#!/bin/sh -e
MOUNT=`stat --print %m .`
TESTDIR=`pwd`
POPD=0
FS_CTX="fscontext=system_u:object_r:test_filesystem_file_t:s0"
# To run individual tests on NFS with -v option:
RUN_TEST=$1
V=$2

function err_exit() {
    set +e # Turn off exit on error
    if [ $POPD -eq 1 ]; then
        echo "Test failed on line: $1 - Closing down NFS"
        popd >/dev/null 2>&1
    else
        echo "Error on line: $1 - Closing down NFS"
    fi
    umount /mnt/selinux-testsuite
    exportfs -u localhost:$MOUNT
    rmdir /mnt/selinux-testsuite
    systemctl stop nfs-server
    echo "NFS Closed down"
    exit 1
}

trap 'err_exit $LINENO' ERR

function run_test() {
    trap 'err_exit $LINENO' ERR

    # Make all required for tests
    make -C tests/fs_filesystem
    if [ $2 ]; then
        cd tests/$1
        ./test $2
        cd ../../
    else
        cd tests
        ./nfsruntests.pl $1/test
        cd ../
    fi
    if [ $POPD -eq 1 ]; then
        popd >/dev/null 2>&1
        umount /mnt/selinux-testsuite
    fi
    exportfs -u localhost:$MOUNT
    rmdir /mnt/selinux-testsuite
    systemctl stop nfs-server
    echo "NFS test $1 complete"
    exit 0
}

# Required by nfs_filesystem/test
export NFS_TESTDIR=$TESTDIR
export NFS_MOUNT=$MOUNT
#
systemctl start nfs-server
# Run the testsuite on a labeled NFS mount.
exportfs -o rw,no_root_squash,security_label localhost:$MOUNT
mkdir -p /mnt/selinux-testsuite
#
if [ $RUN_TEST ] && [ $RUN_TEST = 'nfs_filesystem' ]; then
    run_test $RUN_TEST $V
fi
#
echo "Run selinux-testsuite with no NFS mount context option"
mount -t nfs -o vers=4.2 localhost:$TESTDIR /mnt/selinux-testsuite
pushd /mnt/selinux-testsuite >/dev/null 2>&1
POPD=1
if [ $RUN_TEST ]; then
    run_test $RUN_TEST $V
else
    make -C policy load
    make -C tests test
fi
POPD=0
popd >/dev/null 2>&1
umount /mnt/selinux-testsuite
#
echo -e "Run 'filesystem' tests with mount context option:\n\t$FS_CTX"
mount -t nfs -o vers=4.2,$FS_CTX localhost:$TESTDIR /mnt/selinux-testsuite
pushd /mnt/selinux-testsuite >/dev/null 2>&1
POPD=1
cd tests
./nfsruntests.pl filesystem/test
cd ../
POPD=0
popd >/dev/null 2>&1
umount /mnt/selinux-testsuite
#
echo -e "Run 'fs_filesystem' tests with mount context option:\n\t$FS_CTX"
mount -t nfs -o vers=4.2,$FS_CTX localhost:$TESTDIR /mnt/selinux-testsuite
pushd /mnt/selinux-testsuite >/dev/null 2>&1
POPD=1
cd tests
./nfsruntests.pl fs_filesystem/test
cd ../
POPD=0
popd >/dev/null 2>&1
umount /mnt/selinux-testsuite
#
echo "Run NFS context specific tests"
cd tests
./nfsruntests.pl nfs_filesystem/test
cd ../
#
make -C policy unload
exportfs -u localhost:$MOUNT
rmdir /mnt/selinux-testsuite
systemctl stop nfs-server
echo "NFS tests successfully completed"

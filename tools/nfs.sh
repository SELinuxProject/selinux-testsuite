#!/bin/sh -e
MOUNT=`stat --print %m .`
TESTDIR=`pwd`
MAKE_TEST=0

function err_exit() {
    if [ $MAKE_TEST -eq 1 ]; then
        echo "Closing down NFS"
        popd
    else
        echo "Error on line: $1 - Closing down NFS"
    fi
    umount /mnt/selinux-testsuite
    exportfs -u localhost:$MOUNT
    rmdir /mnt/selinux-testsuite
    systemctl stop nfs-server
    exit 1
}

trap 'err_exit $LINENO' ERR

systemctl start nfs-server
# Run the full testsuite on a labeled NFS mount.
exportfs -orw,no_root_squash,security_label localhost:$MOUNT
mkdir -p /mnt/selinux-testsuite
mount -t nfs -o vers=4.2 localhost:$TESTDIR /mnt/selinux-testsuite
pushd /mnt/selinux-testsuite
MAKE_TEST=1
make test
MAKE_TEST=0
popd
umount /mnt/selinux-testsuite

# Test context mounts when exported with security_label.
mount -t nfs -o vers=4.2,context=system_u:object_r:etc_t:s0 localhost:$TESTDIR /mnt/selinux-testsuite
echo "Testing context mount of a security_label export."
fctx=`secon -t -f /mnt/selinux-testsuite`
if [ "$fctx" != "etc_t" ]; then
    echo "Context mount failed: got $fctx instead of etc_t."
    err_exit $LINENO
fi
umount /mnt/selinux-testsuite
exportfs -u localhost:$MOUNT

# Test context mounts when not exported with security_label.
exportfs -orw,no_root_squash localhost:$MOUNT
mount -t nfs -o vers=4.2,context=system_u:object_r:etc_t:s0 localhost:$TESTDIR /mnt/selinux-testsuite
echo "Testing context mount of a non-security_label export."
fctx=`secon -t -f /mnt/selinux-testsuite`
if [ "$fctx" != "etc_t" ]; then
    echo "Context mount failed: got $fctx instead of etc_t."
    err_exit $LINENO
fi
umount /mnt/selinux-testsuite

# Test non-context mount when not exported with security_label.
mount -t nfs -o vers=4.2 localhost:$TESTDIR /mnt/selinux-testsuite
echo "Testing non-context mount of a non-security_label export."
fctx=`secon -t -f /mnt/selinux-testsuite`
if [ "$fctx" != "nfs_t" ]; then
    echo "Context mount failed: got $fctx instead of nfs_t."
    err_exit $LINENO
fi
umount /mnt/selinux-testsuite

# All done.
echo "Done"
exportfs -u localhost:$MOUNT
rmdir /mnt/selinux-testsuite
systemctl stop nfs-server

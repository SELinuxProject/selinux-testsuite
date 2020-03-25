package Filesystem;
use Exporter qw(import);
our @EXPORT_OK =
  qw(check_config udisks2_stop udisks2_restart get_loop_dev attach_dev make_fs mk_mntpoint_1 mk_mntpoint_2 cleanup cleanup1 reaper nfs_gen_opts);

sub check_config {
    my ( $base, $fanotify_fs, $nfs_enabled, $vfat_enabled ) = @_;

    $tst_count = 0;

    # From kernel 5.4 support for fanotify(7) LSM hooks
    $kvercur = `uname -r`;
    chomp($kvercur);
    $kverminstream = "5.4";
    $watch         = 0;

    $result = `$base/../kvercmp $kvercur $kverminstream`;
    if ( $result > 0 && -e $fanotify_fs ) {
        $watch = 1;
        $tst_count += 11;
    }

    $name_trans        = 0;
    $pol_vers          = `checkpolicy -V | cut -f 1 -d ' '`;
    $mod_pol_vers      = `checkmodule -V | cut -f 2 -d '-'`;
    $max_kernel_policy = `cat /sys/fs/selinux/policyvers`;

    if ( not $vfat_enabled ) {
        if (    $mod_pol_vers >= 11
            and $pol_vers >= 25
            and $max_kernel_policy >= 25 )
        {
            $name_trans = 1;
            $tst_count += 2;
        }
    }

    $type_trans = 0;
    if ( not $vfat_enabled ) {
        $type_trans = 1;
        $tst_count += 1;
    }

    return ( $tst_count, $watch, $name_trans, $type_trans );
}

# Stop the udisks(8) daemon, then restart on exit.
sub udisks2_stop {
    $status = 0;

    if ( -e "/usr/bin/systemctl" ) {
        $u_status_cmd = "/usr/bin/systemctl status udisks2 >& /dev/null";
        $u_stop_cmd   = "/usr/bin/systemctl stop udisks2 >& /dev/null";
    }
    elsif ( -e "/usr/sbin/service" ) {
        $u_status_cmd = "/usr/sbin/service udisks2 status >& /dev/null";
        $u_stop_cmd   = "/usr/sbin/service udisks2 stop >& /dev/null";
    }

    if ($u_status_cmd) {
        $result = system("$u_status_cmd");
        $status = $result >> 8;
        if ( $status eq 0 ) {
            print "Stopping udisks2 service for these tests.\n";
            system("$u_stop_cmd");
            $status = 3;
        }
        else {
            $status = 4;
        }
    }
    return $status;
}

sub udisks2_restart {
    my ($status) = @_;

    if ( $status eq 3 ) {
        print "Restarting udisks2 service.\n";
        if ( -e "/usr/bin/systemctl" ) {
            system("/usr/bin/systemctl start udisks2 >& /dev/null");
        }
        elsif ( -e "/usr/sbin/service" ) {
            system("/usr/sbin/service udisks2 start >& /dev/null");
        }
    }
}

sub get_loop_dev {
    my ( $dev_list, $dev_count ) = @_;

    system("udevadm settle");
    print "Finding free /dev/loop entry\n";
    $new_dev = `losetup -f 2>/dev/null`;
    chomp($new_dev);
    if ( $new_dev eq "" ) {
        print "losetup failed to obtain /dev/loop entry\n";
    }

    # Keep list of devices for cleanup later
    if ( $dev_count eq 0 ) {
        $dev_list[$dev_count] = $new_dev;
        $dev_count += 1;
    }
    elsif ( $new_dev ne $dev_list[ $dev_count - 1 ] ) {
        $dev_list[$dev_count] = $new_dev;
        $dev_count += 1;
    }
    return ( $new_dev, $dev_count );
}

sub attach_dev {
    my ( $att_dev, $base ) = @_;
    system("udevadm settle");
    print "Attaching $base/fstest to $att_dev\n";
    $result = system("losetup $att_dev $base/fstest 2>/dev/null");
    if ( $result != 0 ) {
        print "Failed to attach $base/fstest to $att_dev\n";
    }
    system("udevadm settle");
}

sub make_fs {
    my ( $mk_type, $mk_dev, $mk_dir ) = @_;
    print "Create $mk_dir/fstest with dd\n";
    $result =
      system(
        "dd if=/dev/zero of=$mk_dir/fstest bs=4096 count=4096 2>/dev/null");
    if ( $result != 0 ) {
        print "dd failed to create $mk_dir/fstest\n";
    }

    attach_dev( $mk_dev, $mk_dir );

    print "Make $mk_type filesystem on $mk_dev\n";
    $result = system("mkfs.$mk_type $mk_dev >& /dev/null");
    if ( $result != 0 ) {
        system("losetup -d $mk_dev 2>/dev/null");
        print "mkfs.$mk_type failed to create filesystem on $mk_dev\n";
    }
}

sub mk_mntpoint_1 {
    my ($path) = @_;
    system("rm -rf $path/mp1 2>/dev/null");
    system("mkdir -p $path/mp1 2>/dev/null");
}

sub mk_mntpoint_2 {
    my ($path) = @_;
    system("rm -rf $path/mp2 2>/dev/null");
    system("mkdir -p $path/mp2 2>/dev/null");
}

sub cleanup {
    my ($base) = @_;
    system("rm -rf $base/fstest 2>/dev/null");
    system("rm -rf $base/mntpoint 2>/dev/null");
}

sub cleanup1 {
    my ( $base, $d ) = @_;
    system("udevadm settle");
    system("losetup -d $d 2>/dev/null");
    system("udevadm settle");
    system("rm -rf $base/fstest 2>/dev/null");
    system("rm -rf $base/mntpoint 2>/dev/null");
}

# Cleanup any attached /dev/loop entries
sub reaper {
    my ( $dev_list, $base, $v ) = @_;

    foreach my $n (@dev_list) {
        system("udevadm settle");
        system("$base/grim_reaper -t $n $v 2>/dev/null");
    }
}

sub nfs_gen_opts {

    my ( $type, $base, $filesys ) = @_;

    $mnt_entry = `findmnt -fUln -t $type -T $base`;
    my @mnt_item = split " ", $mnt_entry;

    $tgt  = $mnt_item[0];
    $src  = $mnt_item[1];
    $type = $mnt_item[2];

    # Use a common $dev entry for all tests
    $dev = "$src/tests/$filesys/mntpoint";

    # Build mandatory nfs options, some of which mount.nfs(8) would resolve
    ($clientaddr) = ( $mnt_item[3] =~ /clientaddr=([^,\/]+)/ );
    chomp($clientaddr);
    $clientaddr = "clientaddr=$clientaddr";

    # Remove items that could match e.g. clientaddr, addr
    $mnt_item[3] =~ s/clientaddr=$clientaddr/xxxx/i;
    ($addr) = ( $mnt_item[3] =~ /addr=([^,\/]+)/ );
    chomp($addr);
    $addr = "addr=$addr";
    $mnt_item[3] =~ s/addr=$addr/xxxx/i;
    ($proto) = ( $mnt_item[3] =~ /proto=([^,\/]+)/ );
    chomp($proto);
    $proto = "proto=$proto";
    ($vers) = ( $mnt_item[3] =~ /vers=([^,\/]+)/ );
    chomp($vers);
    $vers = "vers=$vers";

    $fs_context    = " ";
    $fscontext     = " ";
    $seclabel      = " ";
    $seclabel_type = 0;     # No seclabel = 0, fscontext = 1, context = 2
    ($fscontext) = ( $mnt_item[3] =~ /fscontext=([^,\/]+)/ );
    $mnt_item[3] =~ s/fscontext=$fs_context/xxxx/i;
    ($context) = ( $mnt_item[3] =~ /context=([^,\/]+)/ );

    if ( defined $fscontext ) {
        $seclabel      = "fscontext=$fscontext";
        $seclabel_type = 1;
    }
    elsif ( defined $context ) {
        $seclabel      = "context=$context";
        $seclabel_type = 2;
    }
    chomp($seclabel);

    # Use a common set of NFS options. Note fsconfig(2) returns
    # 'Invalid argument' if a blank entry (as $seclabel can be) is passed
    # as a parameter (as NFS sees this as an invalid option).
    if ( $seclabel eq " " ) {
        $nfs_mount_opts = "$vers,$proto,$clientaddr,$addr";
    }
    else {
        $nfs_mount_opts = "$vers,$proto,$clientaddr,$addr,$seclabel";
    }

    # Build option for testing 'SELinux: mount invalid. Same superblock,...'
    # that returns EBUSY. Depends on what the initial mount set as its value
    $inval_seclabel = $seclabel;
    if ( $seclabel_type eq 0 ) {
        $inval_seclabel = "context=system_u:object_r:test_filesystem_file_t:s0";
    }
    elsif ( $seclabel_type eq 1 ) {
        $inval_seclabel =~ s/fscontext/context/i;
    }
    elsif ( $seclabel_type eq 2 ) {
        $inval_seclabel =~ s/context/fscontext/i;
    }
    $nfs_inval_mount_opts = "$vers,$proto,$clientaddr,$addr,$inval_seclabel";

    return ( $dev, $nfs_mount_opts, $nfs_inval_mount_opts, $seclabel_type );
}

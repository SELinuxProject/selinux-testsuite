Basic SELinux Regression Test Suite for the Linux Kernel
===============================================================================
https://github.com/SELinuxProject/selinux-testsuite

This directory contains the functional test suite for the LSM-based SELinux
security module.  Please refer to the test report available in `doc/tests` for
complete documentation for this test suite.  This README only covers a subset
of that report, specifically the sections on running the tests and adding new
tests.

## Prerequisites

### Kernel Configuration

Your kernel should have been built with the configuration options specified by
the `defconfig` file in this directory to run this testsuite.  If you already
have a working kernel configuration you can merge the provided `defconfig` file
with your existing kernel `.config` file (or one generated via e.g.
`make localmodconfig` or other commands) using the kernel 
`./scripts/kconfig/merge_config.sh` script as follows:

	# cd linux
	# ./scripts/kconfig/merge_config.sh .config /path/to/selinux-testsuite/defconfig

Some of the config options may not be set in the final config because they are
unnecessary based on your base config, e.g. you only need the
`CONFIG_*_FS_SECURITY` option to be enabled for the particular filesystem type
being used for the testing.

Do not set `CONFIG_SECURITY_SELINUX_POLICYDB_VERSION_MAX`; it is an option for
legacy distributions (Fedora 3 and 4).

You should not enable any other security modules in your kernel configuration
unless you use the `security=` option to select a module at boot time.  Only
one primary security module may be active at a time.

### Userland and Base Policy

On a Fedora/RHEL based system the testsuite has the following userspace
dependencies beyond a minimal install (other Linux distributions should have
similar dependencies):

* perl-Test  _(test harness used by the testsuite)_
* perl-Test-Harness _(test harness used by the testsuite)_
* perl-Test-Simple _(for `Test::More`)_
* selinux-policy-devel _(to build the test policy)_
* gcc _(to build the test programs)_
* libselinux-devel _(to build some of the test programs)_
* net-tools _(for `ifconfig`, used by `capable_net/test`)_
* netlabel\_tools _(to load NetLabel configuration during `inet_socket` tests)_
* iptables _(to load the `iptables SECMARK` rules during `inet_socket` tests)_
* lksctp-tools-devel _(to build the SCTP test programs)_
* attr _(tools used by the overlayfs tests)_
* libbpf-devel _(tools used by the bpf tests)_
* keyutils-libs-devel _(tools used by the keys tests)_
* kernel-devel _(used by the kernel module tests)_
* quota, xfsprogs-devel and libuuid-devel _(used by the filesystem tests)_

On a modern Fedora system you can install these dependencies with the
following command:

	# dnf install perl-Test \
		perl-Test-Harness \
		perl-Test-Simple \
		selinux-policy-devel \
		gcc \
		libselinux-devel \
		net-tools \
		netlabel_tools \
		iptables \
		lksctp-tools-devel \
		attr \
		libbpf-devel \
		keyutils-libs-devel \
		kernel-devel \
		quota \
		xfsprogs-devel \
		libuuid-devel

The testsuite requires a pre-existing base policy configuration of SELinux,
using either the old example policy or the reference policy as the baseline.
It also requires the core SELinux userland packages (`libsepol`, `checkpolicy`,
`libselinux`, `policycoreutils`, and if using modular policy, `libsemanage`)
to be installed.  The test scripts also rely upon the SELinux extensions being
integrated into the `coreutils` package, with support for the `chcon` and
`runcon` commands as well as the SELinux options to existing utilities such as
`ls` and `mkdir`.

If the base distribution does not include the SELinux userland, then the
source code for the core SELinux userland packages can be obtained from:

* https://github.com/SELinuxProject/selinux/releases

If the base distribution does not include a policy configuration, then
the reference policy can be obtained from:

* https://github.com/SELinuxProject/refpolicy/releases

### Optional Prerequisites

#### InfiniBand

The InfiniBand tests require specialized hardware and are not enabled by
default.  If you have InfiniBand hardware on your system and wish to enable
the InfiniBand tests you will need to install some additional packages, the
list below is for Fedora/RHEL but other Linux distributions should have
similar packages:

* libibverbs-devel _(to build the `ibpkey` test program)_
* infiniband-diags _(for `smpquery` used by `infiniband_endport/test`)_

On a modern Fedora system you can install these dependencies with the
following command:

	# dnf install libibverbs-devel infiniband-diags

Once the necessary packages have been installed, the tests need to be enabled
and configured for your specific hardware configuration.  The test
configuration files are below, and each includes comments to help configure
the tests:

	tests/infiniband_pkey/ibpkey_test.conf
	tests/infiniband_endport/ibendport_test.conf

#### NFS

It is possible to run most of the tests within a labeled NFS mount in
order to exercise the NFS security labeling functionality.  Certain
tests have been excluded from such testing due to differences between
NFS and local filesystems; these tests will be automatically skipped.

You will need to install an additional package, the package below
is for Fedora/RHEL but other Linux distributions should have a similar
package:

* nfs-utils _(for `nfsd', `exportfs', and other NFS-related programs)_

On a modern Fedora system you can install this dependency with the
following command:

	# dnf install nfs-utils

If your distribution does not use systemd as its init system, you will
need to customize the nfs.sh script found in the tools directory to
correctly start and stop the nfs server.  You may also choose to not
start/stop the nfs-server as part of the script by removing those lines
if you are already using NFS for other reasons.

Before running the tests in a labeled NFS mount, first ensure that you
can run them successfully on a local filesystem following the standard
instructions further below.  Any failures that occur on a local
filesystem should also typically be expected when running over NFS.

To run the tests within a labeled NFS mount, you can run the
nfs.sh script while in the selinux-testsuite directory:

       # cd selinux-testsuite
       # ./tools/nfs.sh

The script will start the nfs-server, export the mount containing the
testsuite directory with the security_label option to localhost, mount
it via NFSv4.2 on /mnt/selinux-testsuite, switch to that directory,
and run the testsuite there.  After running the testsuite, the script
will also perform tests of context mounts with and without the
security_label export option and will test default NFS file labeling
in the absence of any options.  When finished, it will unmount and
unexport the mount and then stop the nfs-server.

There is also an option to allow individual tests to be run on NFS as
shown by the following example (ensure a valid policy is loaded):

       # cd selinux-testsuite
       # ./tools/nfs.sh nfs_filesystem -v

Any test will then be run on a security_label exported filesystem without
any *context= option set.

## Running the Tests

Create a shell with the `unconfined_r` or `sysadm_r` role and the Linux
superuser identity, e.g.:

	# newrole -r sysadm_r # -strict or -mls policy only
	# su

Check whether the SELinux kernel is in enforcing mode by running `getenforce`.
If it is in permissive mode, toggle it into enforcing mode by running
`setenforce 1`.

Ensure that `expand-check = 0` in `/etc/selinux/semanage.conf`; if not, edit it
accordingly.

To run the test suite, you can just do a `make test` from the top-level
directory or you can follow these broken-out steps:

1) Load the test policy configuration as follows:

	`# make -C policy load`

2) Build and run the test suite from the tests subdirectory as follows:

	`# make -C tests test`

3) Unload the test policy configuration as follows:

	`# make -C policy unload`

The broken-out steps allow you to run the tests multiple times without
loading policy each time.

Note that if leaving the test policy in-place for further testing, the
policy build process changes a boolean:
   On policy load:   setsebool allow_domain_fd_use=0
   On policy unload: setsebool allow_domain_fd_use=1
The consequence of this is that after a system reboot, the boolean
defaults to true. Therefore if running the fdreceive or binder tests,
reset the boolean to false, otherwise some tests will fail.

4) Review the test results.

As each test script is run, the name of the script will be displayed followed
by a status.  After running all of the test scripts, a summary of the test
results will be displayed.  If all tests were successful, something similar to
the following summary will be displayed (the specific numbers will vary):

	All tests successful.
	Files=7, Tests=16, 2 wallclock secs ( 0.17 cusr + 0.12 csys = 0.29 CPU)

Otherwise, if one or more tests failed, the script will report statistics on
the number of tests that succeeded and will include a table summarizing which
tests had failed.  The output will be similar to the following text, which
shows that a total of three tests have failed:

	Failed Test  Status Wstat Total Fail  Failed  List of failed
	-------------------------------------------------------------------------------
	entrypoint/test               2    1  50.00%  1
	inherit/test                  3    2  66.67%  1-2
	Failed 2/7 test scripts, 71.43% okay. 3/16 subtests failed, 81.25% okay.
	make: *** [test] Error 255

You can also run individual test scripts by hand, e.g. running
`./entrypoint/test`, to see the raw output of the test script.  This is
particularly useful if a particular test within a given script fails in order
to help identify the cause.  When run by hand, the test script displays the
expected number of tests, a status for each test, and any error messages from
the test script or its helper programs.

Please report any failures to the selinux@vger.kernel.org mailing list,
including a copy of the test summary output, the raw output from test scripts
that failed, a description of your base platform, and the particular release
of SELinux that you are using.

## Adding New Tests

The functional test suite is not yet complete, so we still need additional
tests to be written.  See the GitHub issues tracker for the list of tests that
still need to be written:

* https://github.com/SELinuxProject/selinux-testsuite/issues

To add a new test, create a new test policy file and a new test script.  The
new test policy file should be added to the policy directory and should
contain a set of test domains and types specifically designed for the test.
For the test script, create a new subdirectory in the tests subdirectory,
populate it with at least a Makefile and a test perl script and add it to the
`SUBDIRS` definition in the Makefile file.

The Makefile must contain 'all' and 'clean' targets, even if they are empty,
to support the build system.  The test script must run with no arguments and
must comply with the perl automated test format.  The simplest way to comply
with the Perl automated test format is to use the Perl Test module.  To do
this, first include the following statement at the top of the test Perl script:

	use Test;

Next, include a declaration that specifies how many tests the script will run;
the statement to include will be similar to:

	BEGIN { plan tests => 2}    # run two tests

You can then use the 'ok' subroutine to print results:

	ok(1);           # success
	ok(0);           # failure
	ok(0,1);         # failure, got '0', expected '1'
	ok($results, 0); # success if $results == 0, failure otherwise

Standard error is ignored.

In general, the scripts need to know where they are located so that they can
avoid hard-coded paths.  Use the following line of Perl to establish a base
directory (based on the path of the script executable).  This won't always be
accurate, but will work for this test harness/configuration.

	$basedir = $0;  $basedir =~ s|(.*)/[^/]*|$1|;

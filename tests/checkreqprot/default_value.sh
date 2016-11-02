#!/bin/sh

mydir=$(dirname $0)
DISTRO=$($mydir/../os_detect)
kvercmp=$mydir/../kvercmp

if [ "$($kvercmp $(uname -r) 4.4)" -ge 0 ] ||
	[ "$DISTRO" = "RHEL7" -a "$($kvercmp $(uname -r) 3.10.0-327)" -ge 0 ]; then
	grep -q 0 /sys/fs/selinux/checkreqprot 2> /dev/null
	exit $?
fi
exit 0

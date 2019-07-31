#!/bin/sh

mydir="$(dirname $0)"
DISTRO="$("$mydir/../os_detect")"

if "$mydir/../kver_ge" 4.4 ||
	{ [ "$DISTRO" = "RHEL7" ] && "$mydir/../kver_ge" 3.10.0-327; }; then
	grep -q 0 /sys/fs/selinux/checkreqprot 2> /dev/null
	exit $?
fi
exit 0

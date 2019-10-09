#!/bin/sh

binder_dir=$(dirname $0)
kvercmp=$binder_dir/../kvercmp

# If < 5.4 then /dev/binder is automatically assigned by binder driver
# when CONFIG_ANDROID_BINDER_DEVICES="binder"
if [ "$($kvercmp $(uname -r) 5.4)" -lt 0 ]; then
	$binder_dir/check_binder $1 2>/dev/null
	rc=$?
	if [ $rc -ne 1 ]; then
		exit $rc
	fi
	# Have BASE_BINDER_SUPPORT
	if [ "$1" = '-v' ]; then
		echo "Using: /dev/binder"
	fi

	exit $rc
else
	# From 5.4 generate a binder device using binderfs services
	mkdir /dev/binderfs 2>/dev/null
	mount -t binder binder /dev/binderfs -o context=system_u:object_r:device_t:s0 2>/dev/null
	$binder_dir/check_binderfs $1 2>/dev/null
	rc=$?
	if [ $rc -ne 2 ]; then
		umount binder 2>/dev/null
		rmdir /dev/binderfs 2>/dev/null
		exit $rc
	fi
	# Have BINDERFS_SUPPORT
	if [ "$1" = '-v' ]; then
		echo "Using: /dev/binder-test"
	fi

	exit $rc
fi

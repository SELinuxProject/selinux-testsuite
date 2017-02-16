POLDEV ?= /usr/share/selinux/devel

export CFLAGS+=-g -O0 -Wall -D_GNU_SOURCE

DISTRO=$(shell ./os_detect)

SUBDIRS:=domain_trans entrypoint execshare exectrace execute_no_trans \
	fdreceive inherit link mkdir msg open ptrace readlink relabel rename \
	rxdir sem setattr setnice shm sigkill stat sysctl task_create \
	task_setnice task_setscheduler task_getscheduler task_getsid \
	task_getpgid task_setpgid file ioctl capable_file capable_net \
	capable_sys dyntrans dyntrace bounds nnp mmap unix_socket inet_socket \
	overlay checkreqprot mqueue

ifeq ($(shell grep -q cap_userns $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += cap_userns
endif

ifeq ($(shell grep -q icmp_socket $(POLDEV)/include/support/all_perms.spt && grep -q 1 /sys/fs/selinux/policy_capabilities/extended_socket_class && echo true),true)
SUBDIRS += extended_socket_class
endif

ifeq ($(shell grep -q netlink_iscsi_socket $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += netlink_socket
endif

ifeq ($(shell grep -q getrlimit $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += prlimit
endif

ifeq ($(DISTRO),RHEL4)
    SUBDIRS:=$(filter-out bounds dyntrace dyntrans inet_socket mmap nnp overlay unix_socket, $(SUBDIRS))
endif

ifeq ($(DISTRO),RHEL5)
    SUBDIRS:=$(filter-out bounds inet_socket mmap nnp overlay unix_socket, $(SUBDIRS))
endif

ifeq ($(DISTRO),RHEL6)
    SUBDIRS:=$(filter-out nnp overlay, $(SUBDIRS))
endif

ifeq ($(DISTRO),RHEL7)
    SUBDIRS:=$(filter-out netlink_socket, $(SUBDIRS))
endif

.PHONY: all test clean

all:
	@for subdir in $(SUBDIRS); do \
		(cd $$subdir && $(MAKE) $@) || exit 1; \
	done
	chmod +x */test

test: all
	chcon -R -t test_file_t .
	@SUBDIRS="$(SUBDIRS)" PATH=/usr/bin:/bin:/usr/sbin:/sbin ./runtests.pl

clean:
	@for subdir in $(SUBDIRS); do \
		(cd $$subdir && $(MAKE) $@) || exit 1; \
	done

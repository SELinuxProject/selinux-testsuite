RHEL_VER=$(shell cat /etc/redhat-release)
RHEL_VER_PREFIX=Red Hat Enterprise Linux Server release 

SUBDIRS_COMMON:=domain_trans entrypoint execshare exectrace execute_no_trans fdreceive inherit link mkdir msg open ptrace readlink relabel rename rxdir sem setattr setnice shm sigkill stat sysctl task_create task_setnice task_setscheduler task_getscheduler task_getsid task_getpgid task_setpgid wait file ioctl capable_file capable_net capable_sys

SUBDIRS:= $(SUBDIRS_COMMON) dyntrans dyntrace bounds nnp

ifeq ($(RHEL_VER_PREFIX)4, $(findstring $(RHEL_VER_PREFIX)4, $(RHEL_VER)))
    SUBDIRS:=$(SUBDIRS_COMMON)
endif

ifeq ($(RHEL_VER_PREFIX)5, $(findstring $(RHEL_VER_PREFIX)5, $(RHEL_VER)))
    SUBDIRS:=$(SUBDIRS_COMMON) dyntrace dyntrans
endif

ifeq ($(RHEL_VER_PREFIX)6, $(findstring $(RHEL_VER_PREFIX)6, $(RHEL_VER)))
    SUBDIRS:=$(SUBDIRS_COMMON) dyntrace dyntrans bounds
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

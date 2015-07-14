
export CFLAGS+=-g -O0 -Wall -D_GNU_SOURCE

DISTRO=$(shell ./os_detect)

SUBDIRS_COMMON:=domain_trans entrypoint execshare exectrace execute_no_trans fdreceive inherit link mkdir msg open ptrace readlink relabel rename rxdir sem setattr setnice shm sigkill stat sysctl task_create task_setnice task_setscheduler task_getscheduler task_getsid task_getpgid task_setpgid wait file ioctl capable_file capable_net capable_sys

SUBDIRS:= $(SUBDIRS_COMMON) dyntrans dyntrace bounds nnp mmap unix_socket inet_socket

ifeq ($(DISTRO),RHEL4)
    SUBDIRS:=$(SUBDIRS_COMMON)
endif

ifeq ($(DISTRO),RHEL5)
    SUBDIRS:=$(SUBDIRS_COMMON) dyntrace dyntrans
endif

ifeq ($(DISTRO),RHEL6)
    SUBDIRS:=$(SUBDIRS_COMMON) dyntrace dyntrans bounds mmap unix_socket inet_socket
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

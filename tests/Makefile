POLDEV ?= /usr/share/selinux/devel

export CFLAGS+=-g -O0 -Wall -D_GNU_SOURCE

DISTRO=$(shell ./os_detect)

SUBDIRS:=domain_trans entrypoint execshare exectrace execute_no_trans \
	fdreceive inherit link mkdir msg open ptrace readlink relabel rename \
	rxdir sem setattr setnice shm sigkill stat sysctl task_create \
	task_setnice task_setscheduler task_getscheduler task_getsid \
	task_getpgid task_setpgid wait file ioctl capable_file capable_net \
	capable_sys dyntrans dyntrace bounds nnp mmap unix_socket inet_socket \
	overlay

ifeq ($(shell grep -q cap_userns $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += cap_userns
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

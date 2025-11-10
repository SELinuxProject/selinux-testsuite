PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
SBINDIR ?= $(PREFIX)/sbin
POLDEV ?= $(PREFIX)/share/selinux/devel
INCLUDEDIR ?= $(PREFIX)/include
SELINUXFS ?= /sys/fs/selinux
FILESYSTEMS ?= ext4 xfs jfs vfat btrfs

export CFLAGS+=-g -O0 -Wall -D_GNU_SOURCE

DISTRO=$(shell ./os_detect)
SELINUXFS := $(shell cat /proc/mounts | grep selinuxfs | cut -f 2 -d ' ')
CHECKPOLICY = $(BINDIR)/checkpolicy
CHECKMODULE = $(BINDIR)/checkmodule

POL_VERS := $(shell $(CHECKPOLICY) -V |cut -f 1 -d ' ')
MOD_POL_VERS := $(shell $(CHECKMODULE) -V |cut -f 2 -d '-')
MAX_KERNEL_POLICY := $(shell cat $(SELINUXFS)/policyvers)
POL_TYPE := $(shell ./pol_detect $(SELINUXFS))

# Filter out unavailable filesystems
FILESYSTEMS := $(foreach fs,$(FILESYSTEMS),$(shell modprobe $(fs) > /dev/null 2>&1 && echo $(fs)))

SUBDIRS:= domain_trans entrypoint execshare exectrace execute_no_trans \
	fdreceive inherit link mkdir msg open ptrace readlink relabel rename \
	rxdir sem setattr setnice shm sigkill stat sysctl task_create \
	task_setnice task_setscheduler task_getscheduler task_getsid \
	task_getpgid task_setpgid file ioctl capable_file capable_net \
	capable_sys dyntrans dyntrace bounds nnp_nosuid mmap unix_socket \
	inet_socket/tcp inet_socket/udp overlay checkreqprot mqueue \
	mac_admin atsecure infiniband_endport infiniband_pkey

ifeq ($(shell grep -q cap_userns $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifneq ($(shell ./kvercmp $$(uname -r) 4.7),-1)
SUBDIRS += cap_userns
endif
endif

ifeq ($(shell grep -q icmp_socket $(POLDEV)/include/support/all_perms.spt && grep -q 1 $(SELINUXFS)/policy_capabilities/extended_socket_class && echo true),true)
SUBDIRS += extended_socket_class
endif

ifeq ($(shell grep -q corenet_sctp_bind_all_nodes $(POLDEV)/include/kernel/corenetwork.if && grep -q 1 $(SELINUXFS)/policy_capabilities/extended_socket_class && echo true),true)
ifneq ($(shell ./kvercmp $$(uname -r) 4.20.17),-1)
SUBDIRS += sctp
endif
endif

ifeq ($(shell grep -q netlink_iscsi_socket $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += netlink_socket
endif

ifeq ($(shell grep -q getrlimit $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += prlimit
endif

ifeq ($(shell grep -q binder $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifeq ($(shell grep -qs 'struct binder_fd_object' $(INCLUDEDIR)/linux/android/binder.h && echo true),true)
SUBDIRS += binder
endif
endif

ifeq ($(shell grep -q bpf $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifneq ($(shell ./kvercmp $$(uname -r) 4.15),-1)
SUBDIRS += bpf
export CFLAGS += -DHAVE_BPF
endif
endif

ifeq ($(shell grep -q all_key_perms $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += keys
endif

ifeq ($(shell grep -q key_socket $(POLDEV)/include/support/all_perms.spt && test -e $(INCLUDEDIR)/keyutils.h && echo true),true)
SUBDIRS += key_socket
endif

ifeq ($(shell [ $(MAX_KERNEL_POLICY) -ge 32 ] && echo true),true)
ifeq ($(shell [ $(POL_TYPE) = 'MLS' ] || [ $(POL_TYPE) = 'MCS' ] && echo true),true)
ifeq ($(shell [ $(POL_VERS) -ge 32 ] && echo true),true)
SUBDIRS += glblub
endif # POL_VERS
endif # POL_TYPE
endif # MAX_KERNEL_POLICY

ifneq ($(shell ./kvercmp $$(uname -r) 5.2),-1)
SUBDIRS += cgroupfs_label
endif

ifeq ($(shell grep -q all_file_perms.*watch $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS+=notify
endif

ifeq ($(shell grep -q module_load $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifneq ($(shell ./kvercmp $$(uname -r) 4.7),-1)
SUBDIRS+=module_load
endif
endif

ifeq ($(shell grep -q attach_queue $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += tun_tap
endif

ifeq ($(shell grep -q perf_event $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifeq ($(shell grep -q perfmon $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += perf_event
endif
endif

ifeq ($(shell grep -q filesystem $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += $(addprefix filesystem/,$(FILESYSTEMS))
ifeq ($(shell grep -q all_filesystem_perms.*watch $(POLDEV)/include/support/all_perms.spt && echo true),true)
export CFLAGS += -DHAVE_FS_WATCH_PERM
endif
endif

ifeq ($(shell grep -q filesystem $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifneq ($(shell ./kvercmp $$(uname -r) 5.2),-1)
SUBDIRS += $(addprefix fs_filesystem/,$(FILESYSTEMS))
endif
endif

ifeq ($(shell grep -q all_key_perms $(POLDEV)/include/support/all_perms.spt && test -e $(INCLUDEDIR)/linux/watch_queue.h && grep -qs KEYCTL_WATCH_KEY $(INCLUDEDIR)/linux/keyctl.h && echo true),true)
ifneq ($(shell ./kvercmp $$(uname -r) 5.8),-1)
SUBDIRS += watchkey
endif
endif

ifeq ($(shell [ $(MOD_POL_VERS) -ge 18 -a $(MAX_KERNEL_POLICY) -ge 30 ] && echo true),true)
ifeq ($(shell grep -q anon_inode $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifeq ($(shell test -e $(INCLUDEDIR)/linux/userfaultfd.h && echo true),true)
SUBDIRS += userfaultfd
endif
endif
endif

ifeq ($(shell grep -q vsock_socket $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifeq ($(shell grep -qs VMADDR_CID_LOCAL $(INCLUDEDIR)/linux/vm_sockets.h && echo true),true)
SUBDIRS += vsock_socket
endif
endif

# memfd_secret(2) is available since 5.14, but only confined since 6.0
ifneq ($(shell ./kvercmp $$(uname -r) 6.0),-1)
ifeq ($(shell grep -q anon_inode $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += secretmem
endif
endif

# MPTCP is supported since kernel 5.6, but only works with SELinux
# since 6.5
ifneq ($(shell ./kvercmp $$(uname -r) 6.5),-1)
SUBDIRS += inet_socket/mptcp
endif

# nlmsg test dependencies: policy >= 30, nlmsg permission, netlink_xperm capability
ifeq ($(shell [ $(MOD_POL_VERS) -ge 18 -a $(MAX_KERNEL_POLICY) -ge 30 ] && [ -f /sys/fs/selinux/class/netlink_route_socket/perms/nlmsg ] && grep -q 1 $(SELINUXFS)/policy_capabilities/netlink_xperm && echo true),true)
SUBDIRS += nlmsg
endif

# memfd_file test dependencies: memfd_file class, memfd_class capability
ifeq ($(shell [ -d /sys/fs/selinux/class/memfd_file ] && grep -q 1 $(SELINUXFS)/policy_capabilities/memfd_class && echo true),true)
SUBDIRS += memfd
endif

ifeq ($(shell grep -q io_uring $(POLDEV)/include/support/all_perms.spt && echo true),true)
SUBDIRS += io_uring
endif

ifeq ($(DISTRO),RHEL4)
    SUBDIRS:=$(filter-out bounds dyntrace dyntrans inet_socket mmap nnp_nosuid overlay unix_socket, $(SUBDIRS))
endif

ifeq ($(DISTRO),RHEL5)
    SUBDIRS:=$(filter-out bounds inet_socket mmap nnp_nosuid overlay unix_socket atsecure mac_admin, $(SUBDIRS))
endif

ifeq ($(DISTRO),RHEL6)
    SUBDIRS:=$(filter-out nnp_nosuid overlay atsecure, $(SUBDIRS))
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
	@SUBDIRS="$(SUBDIRS)" ./runtests.pl

clean:
	@for subdir in $(SUBDIRS); do \
		(cd $$subdir && $(MAKE) $@) || exit 1; \
	done

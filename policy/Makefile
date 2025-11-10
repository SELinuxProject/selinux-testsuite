
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
SBINDIR ?= $(PREFIX)/sbin
POLDEV ?= $(PREFIX)/share/selinux/devel
SELINUXFS ?= /sys/fs/selinux
SEMODULE = $(SBINDIR)/semodule
CHECKPOLICY = $(BINDIR)/checkpolicy
CHECKMODULE = $(BINDIR)/checkmodule
SUPPORTS_CIL ?= y

DISTRO=$(shell ../tests/os_detect)

POL_VERS := $(shell $(CHECKPOLICY) -V |cut -f 1 -d ' ')
MOD_POL_VERS := $(shell $(CHECKMODULE) -V |cut -f 2 -d '-')
MAX_KERNEL_POLICY := $(shell cat $(SELINUXFS)/policyvers)
POL_TYPE := $(shell ../tests/pol_detect $(SELINUXFS))

TARGETS = \
	test_global.te test_capable_file.te test_capable_net.te \
	test_capable_sys.te test_dyntrace.te test_dyntrans.te \
	test_entrypoint.te test_execshare.te test_exectrace.te \
	test_execute_no_trans.te test_fdreceive.te test_file.te \
	test_inherit.te test_ioctl.te test_ipc.te test_link.te test_mkdir.te \
	test_open.te test_ptrace.te test_readlink.te \
	test_relabel.te test_rename.te test_rxdir.te test_setattr.te \
	test_setnice.te test_sigkill.te test_stat.te test_sysctl.te \
	test_task_create.te test_task_getpgid.te test_task_getsched.te \
	test_task_getsid.te test_task_setpgid.te test_task_setsched.te \
	test_transition.te test_unix_socket.te \
	test_mmap.te test_overlayfs.te test_mqueue.te \
	test_ibpkey.te test_atsecure.te test_cgroupfs.te

ifeq (x$(DISTRO),$(filter x$(DISTRO),xRHEL4 xRHEL5 xRHEL6))
SUPPORTS_CIL = n
endif

ifeq ($(SUPPORTS_CIL),y)
CIL_TARGETS = test_mlsconstrain.cil test_overlay_defaultrange.cil
ifeq ($(shell [ $(MAX_KERNEL_POLICY) -ge 32 ] && echo true),true)
ifeq ($(shell [ $(POL_VERS) -ge 32 ] && echo true),true)
# If other MLS tests get written this can be moved outside of the glblub test
ifeq ($(POL_TYPE), MLS)
CIL_TARGETS += test_glblub.cil
else ifeq ($(POL_TYPE), MCS)
CIL_TARGETS += test_add_levels.cil test_glblub.cil
endif # POL_TYPE
endif # POL_VERS
endif # MAX_KERNEL_POLICY
endif # SUPPORTS_CIL

ifeq ($(shell [ $(POL_VERS) -ge 24 ] && echo true),true)
TARGETS += test_bounds.te test_nnp_nosuid.te
endif

ifeq ($(shell [ $(MOD_POL_VERS) -ge 18 -a $(MAX_KERNEL_POLICY) -ge 30 ] && echo true),true)
TARGETS += test_ioctl_xperms.te
endif

ifeq ($(shell grep -q cap_userns $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_cap_userns.te
endif

ifeq ($(shell grep -q icmp_socket $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_extended_socket_class.te
endif

ifeq ($(shell grep -q netlink_iscsi_socket $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_netlink_socket.te
endif

ifeq ($(shell grep -q getrlimit $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_prlimit.te
endif

ifeq ($(shell grep -q infiniband_endport $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_ibendport.te
endif

ifeq ($(shell grep -q all_file_perms.*map $(POLDEV)/include/support/all_perms.spt && echo true),true)
export M4PARAM = -Dmap_permission_defined
endif

ifeq ($(shell grep -q nnp_transition $(POLDEV)/include/support/all_perms.spt && echo true),true)
export M4PARAM += -Dnnp_nosuid_transition_permission_defined
endif

ifeq ($(shell grep -q binder $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_binder.te
endif

ifeq ($(shell grep -q mac_admin $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_mac_admin.te
endif

ifeq ($(shell grep -q corenet_inout_generic_if $(POLDEV)/include/kernel/corenetwork.if && echo true),true)
TARGETS += test_inet_socket.te
endif

ifeq ($(shell grep -q corenet_sctp_bind_all_nodes $(POLDEV)/include/kernel/corenetwork.if && echo true),true)
TARGETS += test_sctp.te
endif

ifeq ($(shell grep -q bpf $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_bpf.te test_fdreceive_bpf.te test_binder_bpf.te
endif

ifeq ($(shell grep -q all_key_perms $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_keys.te test_watchkey.te
endif

ifeq ($(shell grep -q all_file_perms.*watch $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS+=test_notify.te
endif

ifeq ($(shell grep -q key_socket $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_key_socket.te
endif

ifeq ($(shell grep -q vsock_socket $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_vsock_socket.te
endif

ifeq ($(shell grep -q module_load $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS+=test_module_load.te
endif

ifeq ($(shell grep -q attach_queue $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_tun_tap.te
endif

ifeq ($(shell grep -q perf_event $(POLDEV)/include/support/all_perms.spt && echo true),true)
ifeq ($(shell grep -q perfmon $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_perf_event.te
endif
endif

# Older kernels may still have the legacy lockdown class, so we need to add
# the appropriate rules when the policy declares it.
ifeq ($(shell grep -q lockdown $(POLDEV)/include/support/all_perms.spt && echo true),true)
export M4PARAM += -Dlockdown_defined
endif

ifeq ($(shell grep -q filesystem $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_filesystem.te
ifeq ($(shell [ $(MOD_POL_VERS) -ge 11 -a $(POL_VERS) -ge 25 ] && echo true),true)
TARGETS += test_filesystem_name_trans.te
ifeq ($(shell grep -q all_file_perms.*watch $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS+= test_filesystem_notify.te
endif
endif
endif

# userfaultfd test policy uses also xperms
ifeq ($(shell [ $(MOD_POL_VERS) -ge 18 -a $(MAX_KERNEL_POLICY) -ge 30 ] && echo true),true)
ifeq ($(shell grep -q anon_inode $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_userfaultfd.te
endif
endif

# nlmsg test dependencies: policy >= 30, nlmsg permission, netlink_xperm capability
ifeq ($(shell [ $(MOD_POL_VERS) -ge 18 -a $(MAX_KERNEL_POLICY) -ge 30 ] && [ -f /sys/fs/selinux/class/netlink_route_socket/perms/nlmsg ] && grep -q 1 $(SELINUXFS)/policy_capabilities/netlink_xperm && echo true),true)
TARGETS += test_nlmsg.te
endif

# memfd_file test dependencies: memfd_file class, memfd_class capability
ifeq ($(shell [ -d /sys/fs/selinux/class/memfd_file ] && grep -q 1 $(SELINUXFS)/policy_capabilities/memfd_class && echo true),true)
TARGETS += test_memfd.te
endif

ifeq (x$(DISTRO),$(filter x$(DISTRO),xRHEL4 xRHEL5 xRHEL6))
TARGETS:=$(filter-out test_overlayfs.te test_mqueue.te test_ibpkey.te, $(TARGETS))
endif

ifeq ($(shell grep -q anon_inode $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_secretmem.te
endif

ifeq ($(shell grep -q user_namespace $(POLDEV)/include/support/all_perms.spt && echo true),true)
export M4PARAM += -Duser_namespace_defined
endif

ifeq ($(shell grep -q checkpoint_restore $(POLDEV)/include/support/all_perms.spt && echo true),true)
export M4PARAM += -Dcheckpoint_restore_defined
endif

# conditional xperm dependency: policy >= 34
ifeq ($(shell [ $(POL_VERS) -ge 34 -a $(MAX_KERNEL_POLICY) -ge 34 ] && echo true),true)
TARGETS += test_ioctl_cond_xperms.te
endif

ifeq ($(shell grep -q firmware_load $(POLDEV)/include/support/all_perms.spt && \
             grep -q kexec_image_load $(POLDEV)/include/support/all_perms.spt && \
             grep -q kexec_initramfs_load $(POLDEV)/include/support/all_perms.spt && \
             grep -q policy_load $(POLDEV)/include/support/all_perms.spt && \
             echo true),true)
TARGETS += test_file_load.te
endif

ifeq ($(shell grep -q io_uring $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_io_uring.te
endif

all: build

expand_check:
	# Test for "expand-check = 0" in /etc/selinux/semanage.conf
	@grep -q '^[ \t]*expand-check[ \t]*=[ \t]*0' /etc/selinux/semanage.conf || \
		(echo "ERROR: set 'expand-check = 0' in /etc/selinux/semanage.conf"; \
		 /bin/false)

build: $(TARGETS)
	# General policy build
	@if [ -d $(POLDEV) ]; then \
		mkdir -p test_policy; \
		cp test_policy.if test_policy; \
		set -e; rm -f test_policy.te; \
		cat $(TARGETS) > test_policy/test_policy.te; \
		$(MAKE) -C test_policy -f $(POLDEV)/Makefile test_policy.pp; \
	else \
		echo "ERROR: You must have selinux-policy-devel installed."; \
	fi

load: expand_check all
	# General policy load
	@if /usr/sbin/getsebool allow_domain_fd_use 2> /dev/null; then \
		/usr/sbin/setsebool allow_domain_fd_use=0; \
	fi
	$(SEMODULE) -i test_policy/test_policy.pp $(CIL_TARGETS)

unload:
	# General policy unload
	@if /usr/sbin/getsebool allow_domain_fd_use 2> /dev/null; then \
		/usr/sbin/setsebool allow_domain_fd_use=1; \
	fi
	$(SEMODULE) -r test_policy $(subst .cil,,$(CIL_TARGETS))

clean:
	rm -rf test_policy tmp

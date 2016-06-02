
POLDEV ?= /usr/share/selinux/devel
SEMODULE = /usr/sbin/semodule
CHECKPOLICY = /usr/bin/checkpolicy

RHEL_REL=$(shell rpm -q redhat-release)
RHEL_VERS=$(shell echo $(RHEL_REL) | cut -f3 -d"-" | sed -e "s/[^0-9]*//g")

POL_VERS := $(shell $(CHECKPOLICY) -V |cut -f 1 -d ' ')

TARGETS = \
	test_global.te test_capable_file.te test_capable_net.te \
	test_capable_sys.te test_dyntrace.te test_dyntrans.te \
	test_entrypoint.te test_execshare.te test_exectrace.te \
	test_execute_no_trans.te test_fdreceive.te test_file.te \
	test_inherit.te test_ioctl.te test_ipc.te test_link.te test_mkdir.te \
	test_nnp.te test_open.te test_ptrace.te test_readlink.te \
	test_relabel.te test_rename.te test_rxdir.te test_setattr.te \
	test_setnice.te test_sigkill.te test_stat.te test_sysctl.te \
	test_task_create.te test_task_getpgid.te test_task_getsched.te \
	test_task_getsid.te test_task_setpgid.te test_task_setsched.te \
	test_transition.te test_inet_socket.te test_unix_socket.te \
	test_wait.te test_mmap.te test_overlayfs.te

ifeq ($(shell [ $(POL_VERS) -ge 24 ] && echo true),true)
TARGETS += test_bounds.te
endif

ifeq ($(shell grep -q cap_userns $(POLDEV)/include/support/all_perms.spt && echo true),true)
TARGETS += test_cap_userns.te
endif

ifeq (x$(RHEL_VERS),$(filter x$(RHEL_VERS),x4 x5))
	BUILD_TARGET := build_rhel
	LOAD_TARGET := load_rhel
	UNLOAD_TARGET := unload_rhel
else
	BUILD_TARGET := build_general
	LOAD_TARGET := load_general
	UNLOAD_TARGET := unload_general
endif

all: build

build: $(BUILD_TARGET)
load: $(LOAD_TARGET)
unload: $(UNLOAD_TARGET)

build_rhel: $(TARGETS)
	# RHEL specific policy build
	$(MAKE) -C redhat/$(RHEL_VERS) all

build_general: $(TARGETS)
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

load_rhel: all
	# RHEL specific policy load
	$(MAKE) -C redhat/$(RHEL_VERS) load

load_general: all
	# General policy load
	/usr/sbin/setsebool allow_domain_fd_use=0
	$(SEMODULE) -i test_policy/test_policy.pp

unload_rhel:
	# RHEL specific policy unload
	$(MAKE) -C redhat/$(RHEL_VERS) unload

unload_general:
	# General policy unload
	/usr/sbin/setsebool allow_domain_fd_use=1
	$(SEMODULE) -r test_policy

clean:
	rm -rf test_policy tmp

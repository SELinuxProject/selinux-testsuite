
POLDEV = /usr/share/selinux/devel
SEMODULE = /usr/sbin/semodule
CHECKPOLICY = /usr/bin/checkpolicy

REDHAT_RELEASE=$(shell rpm -q redhat-release)
REDHAT_VERS=$(shell echo $(REDHAT_RELEASE) | cut -f3 -d"-" | sed -e "s/[^0-9]*//g")

POL_VERS := $(shell $(CHECKPOLICY) -V |cut -f 1 -d ' ')

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
	test_transition.te test_wait.te

ifeq ($(shell [ $(POL_VERS) -ge 24 ] && echo true),true)
TARGETS += test_bounds.te
endif

all: $(TARGETS)
ifeq (x$(REDHAT_VERS),$(filter x$(REDHAT_VERS),x4 x5))
	$(MAKE) -C redhat/$(REDHAT_VERS) all
else
	@if [ -d $(POLDEV) ]; then \
		mkdir -p test_policy; \
		cp test_policy.if test_policy; \
		set -e; rm -f test_policy.te; \
		cat $(TARGETS) > test_policy/test_policy.te; \
		$(MAKE) -C test_policy -f $(POLDEV)/Makefile test_policy.pp; \
	else \
		echo "ERROR: You must have selinux-policy-devel installed."; \
	fi
endif

load: all
ifeq (x$(REDHAT_VERS),$(filter x$(REDHAT_VERS),x4 x5))
	$(MAKE) -C redhat/$(REDHAT_VERS) load
else
	/usr/sbin/setsebool allow_domain_fd_use=0
	$(SEMODULE) -i test_policy/test_policy.pp
endif

unload:
ifeq (x$(REDHAT_VERS),$(filter x$(REDHAT_VERS),x4 x5))
	$(MAKE) -C redhat/$(REDHAT_VERS) unload
else
	/usr/sbin/setsebool allow_domain_fd_use=1
	$(SEMODULE) -r test_policy
endif

clean:
	rm -rf test_policy tmp

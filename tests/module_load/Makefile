obj-m = setest_module_load.o setest_module_request.o

TARGETS = finit_load init_load
LDLIBS += -lselinux
KDIR = /lib/modules/$(shell uname -r)/build

# Make sure to use the same compiler as the kernel was built with.
# If the compilers don't match, the build will fail on unsupported compiler
# flags and even if not, the resulting module would likely fail to load.
# If the kernel was compiled with neither GCC nor clang (currently the only
# supported compilers), fall back to the default compiler and hope for the best.
# In all cases allow the user to override the compiler via the KCC variable.
DETECTED_KCC = unknown
ifeq ($(shell grep -qFx CONFIG_CC_IS_GCC=y $(KDIR)/.config && echo true),true)
	DETECTED_KCC = gcc
endif
ifeq ($(shell grep -qFx CONFIG_CC_IS_CLANG=y $(KDIR)/.config && echo true),true)
	DETECTED_KCC = clang
endif
ifneq ($(DETECTED_KCC),unknown)
	KCC ?= $(DETECTED_KCC)
else
	KCC ?= $(CC)
endif

all: $(TARGETS)
	$(MAKE) -C $(KDIR) CC=$(KCC) M=$(PWD)

clean:
	rm -f $(TARGETS)
	$(MAKE) -C $(KDIR) CC=$(KCC) M=$(PWD) clean

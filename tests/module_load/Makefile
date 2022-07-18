obj-m = setest_module_load.o setest_module_request.o

TARGETS = finit_load init_load
LDLIBS += -lselinux
KDIR = /lib/modules/$(shell uname -r)/build

# Make sure to use the same compiler+linker as the kernel was built with.
# If the compilers/linkers don't match, the build could fail on unsupported
# flags and even if not, the resulting module would likely fail to load.
# If the kernel was compiled with a compiler other than GCC or clang or a
# linker other than ld.bfd or ld.lld, fall back to the default compiler/linker
# and hope for the best.
# In all cases allow the user to override the compiler via the KCC/KLD
# variables.

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

DETECTED_KLD = unknown
ifeq ($(shell grep -qFx CONFIG_LD_IS_BFD=y $(KDIR)/.config && echo true),true)
	DETECTED_KLD = ld.bfd
endif
ifeq ($(shell grep -qFx CONFIG_LD_IS_LLD=y $(KDIR)/.config && echo true),true)
	DETECTED_KLD = ld.lld
endif
ifneq ($(DETECTED_KLD),unknown)
	KLD ?= $(DETECTED_KLD)
else
	KLD ?= $(LD)
endif

all: $(TARGETS)
	$(MAKE) -C $(KDIR) CC=$(KCC) LD=$(KLD) M=$(PWD)

clean:
	rm -f $(TARGETS)
	$(MAKE) -C $(KDIR) CC=$(KCC) LD=$(KLD) M=$(PWD) clean

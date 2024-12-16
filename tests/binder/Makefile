# Required for local building
INCLUDEDIR ?= /usr/include

TARGETS = check_binder client manager service_provider
LDLIBS += -lselinux -lrt
DEPS = binder_common.c

ifeq ($(shell test -e $(INCLUDEDIR)/linux/android/binderfs.h && echo true),true)
CFLAGS += -DHAVE_BINDERFS
TARGETS += check_binderfs
endif

ifneq (,$(findstring -DHAVE_BPF,$(CFLAGS)))
	DEPS += ../bpf/bpf_common.c
	LDLIBS += -lbpf
endif

all: $(TARGETS)

clean:
	rm -f $(TARGETS) manager_flag service_provider_flag

$(TARGETS): $(DEPS)

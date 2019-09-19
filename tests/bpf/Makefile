TARGETS = bpf_test
DEPS = bpf_common.c bpf_common.h
LDLIBS += -lselinux -lbpf

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

$(TARGETS): $(DEPS)

TARGETS = client server

ifneq (,$(findstring -DHAVE_BPF,$(CFLAGS)))
	DEPS = ../bpf/bpf_common.c ../bpf/bpf_common.h
	LDLIBS += -lbpf
endif

all: $(TARGETS)

clean:
	rm -f $(TARGETS) test_file test_file2 test_sock flag

client: $(DEPS)

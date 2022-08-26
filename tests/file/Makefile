TARGETS=$(patsubst %.c,%,$(wildcard *.c))
LDLIBS += -lselinux
test_sigiotask: LDLIBS += -lutil
all: $(TARGETS)
clean:
	rm -f $(TARGETS) temp_file temp_file2 temp_file3

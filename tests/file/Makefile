TARGETS=$(patsubst %.c,%,$(wildcard *.c))
LDLIBS += -lselinux
test_sigiotask: LDLIBS += -lutil
all: $(TARGETS)
clean:
	rm -f $(TARGETS)

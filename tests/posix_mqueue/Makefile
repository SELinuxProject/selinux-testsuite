TARGETS=$(patsubst %.c,%,$(wildcard *.c))
LDLIBS += -lselinux -lrt
test_sigiotask: LDLIBS += -lutil
all: $(TARGETS)
clean:
	rm -f $(TARGETS)

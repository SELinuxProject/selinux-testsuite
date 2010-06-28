TARGETS=$(patsubst %.c,%,$(wildcard *.c))
LDLIBS += -lselinux
all: $(TARGETS)
clean:
	rm -f $(TARGETS)

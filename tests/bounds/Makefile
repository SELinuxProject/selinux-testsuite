TARGETS=$(patsubst %.c,%,$(wildcard *.c))

LDLIBS += -lselinux -lpthread

all: $(TARGETS)
clean:
	rm -f $(TARGETS)

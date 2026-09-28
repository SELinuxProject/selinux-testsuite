TARGETS = tun_tap tun_relabel
DEPS = tun_common.c
LDLIBS += -lselinux

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

$(TARGETS): $(DEPS)

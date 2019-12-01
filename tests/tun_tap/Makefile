TARGETS = tun_tap tun_relabel
DEPS = tun_common.c tun_common.h
LDLIBS += -lselinux

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

$(TARGETS): $(DEPS)

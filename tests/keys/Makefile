TARGETS = keyctl keyctl_relabel keyring_service request_keys
LDLIBS += -lselinux -lkeyutils

$(TARGETS): keys_common.h

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

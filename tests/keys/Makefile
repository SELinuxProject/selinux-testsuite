TARGETS = keyctl keyctl_relabel keyring_service request_keys
LDLIBS += -lselinux -lkeyutils

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

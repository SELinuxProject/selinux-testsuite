TARGETS = fsmount fspick move_mount
DEPS = fs_common.c fs_common.h
LDLIBS += -lselinux
USE_FS = ../filesystem

all: $(TARGETS)
	@set -e; for i in $(USE_FS); do $(MAKE) -C $$i all ; done

clean:
	rm -f $(TARGETS)
	@set -e; for i in $(USE_FS); do $(MAKE) -C $$i clean ; done

$(TARGETS): $(DEPS)

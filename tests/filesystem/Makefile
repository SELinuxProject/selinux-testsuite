# Required for local building
#export CFLAGS += -DHAVE_FS_WATCH_PERM

TARGETS = mount umount quotas_test statfs_test create_file_change_context \
fs_relabel check_file_context grim_reaper check_mount_context create_file \
xfs_quotas_test

LDLIBS += -lselinux

ifneq (,$(findstring -DHAVE_FS_WATCH_PERM,$(CFLAGS)))
	TARGETS += fanotify_fs
endif

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

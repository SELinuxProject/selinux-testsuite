#include "fs_common.h"

int fsopen(const char *fs_name, unsigned int flags)
{
	return syscall(__NR_fsopen, fs_name, flags);
}

int fsmount(int fsfd, unsigned int flags, unsigned int ms_flags)
{
	return syscall(__NR_fsmount, fsfd, flags, ms_flags);
}

int fsconfig(int fsfd, unsigned int cmd, const char *key,
	     const void *val, int aux)
{
	return syscall(__NR_fsconfig, fsfd, cmd, key, val, aux);
}

int fspick(int dirfd, const char *pathname, unsigned int flags)
{
	return syscall(__NR_fspick, dirfd, pathname, flags);
}

int move_mount(int from_dfd, const char *from_pathname, int to_dfd,
	       const char *to_pathname, unsigned int flags)
{
	return syscall(__NR_move_mount, from_dfd, from_pathname,
		       to_dfd, to_pathname, flags);
}

int open_tree(int dirfd, const char *pathname, unsigned int flags)
{
	return syscall(__NR_open_tree, dirfd, pathname, flags);
}

int fsconfig_opts(int fd, char *src, char *tgt, char *opts, bool verbose)
{
	int result, i, save_errno, start_count, max_entries = 0;
	int cmd[MAX_OPS];
	char *key[MAX_OPS], *value[MAX_OPS];
	char *src_str = "source";

	/* If src then fsmount(2), else its going to be fspick(2) */
	if (src) {
		cmd[0] = FSCONFIG_SET_STRING;
		key[0] = src_str;
		value[0] = src;
		start_count = 1;
	} else {
		start_count = 0;
	}

	for (i = start_count; i < MAX_OPS; i++) {
		value[i] = strsep(&opts, ",");
		if (!value[i]) {
			max_entries = i + 1;
			break;
		}
		cmd[i] = FSCONFIG_SET_STRING;
	}

	for (i = start_count; value[i] != NULL; i++) {
		key[i] = strsep(&value[i], "=");
		if (!value[i])
			cmd[i] = FSCONFIG_SET_FLAG;
	}

	if (src) {
		cmd[i] = FSCONFIG_CMD_CREATE;
		key[i] = NULL;
		value[i] = NULL;
	} else {
		cmd[i] = FSCONFIG_CMD_RECONFIGURE;
		key[i] = NULL;
		value[i] = NULL;
	}

	for (i = 0; i != max_entries; i++) {
		if (verbose) {
			switch (cmd[i]) {
			case FSCONFIG_CMD_CREATE:
				printf("fsconfig(FSCONFIG_CMD_CREATE, %s, %s, 0)\n",
				       key[i], value[i]);
				break;
			case FSCONFIG_CMD_RECONFIGURE:
				printf("fsconfig(FSCONFIG_CMD_RECONFIGURE, %s, %s, 0)\n",
				       key[i], value[i]);
				break;
			case FSCONFIG_SET_FLAG:
				printf("fsconfig(FSCONFIG_SET_FLAG, %s, %s, 0)\n",
				       key[i], value[i]);
				break;
			case FSCONFIG_SET_STRING:
				printf("fsconfig(FSCONFIG_SET_STRING, %s, %s, 0)\n",
				       key[i], value[i]);
				break;
			}
		}

		result = fsconfig(fd, cmd[i], key[i], value[i], 0);
		save_errno = errno;
		if (result < 0) {
			fprintf(stderr, "Failed fsconfig(2): %s\n",
				strerror(save_errno));
			return save_errno;
		}
	}

	return 0;
}

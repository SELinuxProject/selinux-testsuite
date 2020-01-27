#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/prctl.h>
#include <linux/mount.h>
#include <linux/unistd.h>
#include <selinux/selinux.h>

#ifndef AT_EMPTY_PATH
#define AT_EMPTY_PATH		0x1000
#endif
#ifndef AT_RECURSIVE
#define AT_RECURSIVE		0x8000
#endif

int fsopen(const char *fs_name, unsigned int flags);
int fsmount(int fsfd, unsigned int flags, unsigned int ms_flags);
int fsconfig(int fsfd, unsigned int cmd, const char *key,
	     const void *val, int aux);
int move_mount(int from_dfd, const char *from_pathname, int to_dfd,
	       const char *to_pathname, unsigned int flags);
int open_tree(int dirfd, const char *pathname, unsigned int flags);
int fspick(int dirfd, const char *pathname, unsigned int flags);

#define MAX_OPS 40
int fsconfig_opts(int fd, char *src, char *tgt, char *opts, bool verbose);

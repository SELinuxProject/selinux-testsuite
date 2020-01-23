#ifndef _BINDER_COMMON_H
#define _BINDER_COMMON_H

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <selinux/selinux.h>
#include <linux/android/binder.h>
#if HAVE_BINDERFS
#include <linux/android/binderfs.h>
#endif
#if HAVE_BPF
#include "../bpf/bpf_common.h"
#endif

#define BINDER_DEV "/dev/binder"
#define BINDERFS_DEV "/dev/binderfs"
#define BINDERFS_NAME "binder-test"
#define BINDERFS_CONTROL "/dev/binderfs/binder-control"
#define BINDER_MMAP_SIZE 1024

/* Return codes for check_binder and check_binderfs */
enum {
	BINDER_ERROR = -1,
	NO_BINDER_SUPPORT = 0,
	BASE_BINDER_SUPPORT,
	BINDERFS_SUPPORT,
	BINDER_VER_ERROR
};

#define TEST_SERVICE_MANAGER_HANDLE 0
/* These are the Binder txn->code values used by the Service Provider, Client
 * and Manager to request/retrieve a binder handle or file descriptor.
 */
#define TEST_SERVICE_ADD	240616 /* Sent by Service Provider */
#define TEST_SERVICE_GET	290317 /* Sent by Client */
#define TEST_SERVICE_SEND_FD	311019 /* Sent by Client */

extern bool verbose;

const char *cmd_name(uint32_t cmd);
void print_trans_data(const struct binder_transaction_data *txn_in);
int binder_write(int fd, void *data, size_t len);

enum binder_test_fd_t {
	BINDER_FD,
	BPF_MAP_FD,
	BPF_PROG_FD,
	BPF_TEST
};
extern enum binder_test_fd_t fd_type;

extern char *fd_type_str;

#endif

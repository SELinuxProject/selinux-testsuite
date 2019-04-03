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

#define BINDER_DEV "/dev/binder"
#define BINDERFS_DEV "/dev/binderfs"
#define BINDERFS_NAME "binder-test"
#define BINDERFS_CONTROL "/dev/binderfs/binder-control"
#define BINDER_MMAP_SIZE 1024

#define TEST_SERVICE_MANAGER_HANDLE 0
/* These are the Binder txn->code values used by the Service Provider, Client
 * and Manager to request/retrieve a binder handle or file descriptor.
 */
#define TEST_SERVICE_ADD		240616 /* Sent by Service Provider */
#define TEST_SERVICE_GET		290317 /* Sent by Client */
#define TEST_SERVICE_SEND_CLIENT_SP_FD	120419 /* Sent by Client */

bool verbose;

const char *cmd_name(uint32_t cmd);
void print_trans_data(const struct binder_transaction_data *txn_in);
int binder_write(int fd, void *data, size_t len);

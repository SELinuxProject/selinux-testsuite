#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <selinux/selinux.h>
#include <selinux/context.h>

extern int open_dev(int *fd, char *test_str, bool verbose);
extern int setiff(int fd, struct ifreq *ifr, bool verbose);
/* Persistent state 'op': 0 = unset, 1 = set */
extern int persist(int fd, int op, char *name, bool verbose);
/* Queue state 'op': 0 = IFF_DETACH_QUEUE, 1 = IFF_ATTACH_QUEUE */
extern int tunsetqueue(int fd, int op, char *name, bool verbose);
extern int switch_context(char *newcon, bool verbose);
extern void del_tuntap_name(int fd, char *context, char *name, bool verbose);

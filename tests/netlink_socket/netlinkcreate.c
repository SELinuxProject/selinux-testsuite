#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

struct nameval {
	const char *name;
	const int value;
};

static struct nameval protocols[] = {
	{ "route", NETLINK_ROUTE },
	{ "sock_diag", NETLINK_SOCK_DIAG },
	{ "nflog", NETLINK_NFLOG },
	{ "xfrm", NETLINK_XFRM },
	{ "selinux", NETLINK_SELINUX },
	{ "iscsi", NETLINK_ISCSI },
	{ "audit", NETLINK_AUDIT },
	{ "fib_lookup", NETLINK_FIB_LOOKUP },
	{ "connector", NETLINK_CONNECTOR },
	{ "netfilter", NETLINK_NETFILTER },
	{ "dnrtmsg", NETLINK_DNRTMSG },
	{ "kobject_uevent", NETLINK_KOBJECT_UEVENT },
	{ "generic", NETLINK_GENERIC },
	{ "scsitransport", NETLINK_SCSITRANSPORT },
	{ "rdma", NETLINK_RDMA },
	{ "crypto", NETLINK_CRYPTO },
	{ NULL, 0 }
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int lookup_value(const char *name, const struct nameval *nvlist)
{
	const struct nameval *nv;

	for (nv = nvlist; nv->name; nv++) {
		if (!strcmp(nv->name, name))
			return nv->value;
	}
	return -1;
}

int main(int argc, char **argv)
{
	int sock;
	int protocol;

	if (argc != 2) {
		fprintf(stderr, "usage: %s protocol\n", argv[0]);
		exit(1);
	}

	protocol = lookup_value(argv[1], protocols);
	if (protocol < 0) {
		fprintf(stderr, "%s: unknown protocol %s\n", argv[0], argv[1]);
		exit(1);
	}

	sock = socket(AF_NETLINK, SOCK_DGRAM, protocol);
	if (sock < 0) {
		fprintf(stderr, "%s: socket(AF_NETLINK, SOCK_DGRAM, %s/%d): %s\n",
			argv[0], argv[1], protocol, strerror(errno));
		exit(1);
	}
	close(sock);
	exit(0);
}

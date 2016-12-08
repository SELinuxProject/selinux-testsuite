#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

struct nameval {
	const char *name;
	const int value;
};

static struct nameval domains[] = {
	{ "inet", AF_INET },
	{ "inet6", AF_INET6 },
	{ "bluetooth", AF_BLUETOOTH },
	{ "alg", AF_ALG },
	{ NULL, 0 }
};

static struct nameval types[] = {
	{ "stream", SOCK_STREAM },
	{ "dgram", SOCK_DGRAM },
	{ "seqpacket", SOCK_SEQPACKET },
	{ "raw", SOCK_RAW },
	{ NULL, 0 }
};

static struct nameval protocols[] = {
	{ "icmp", IPPROTO_ICMP },
	{ "icmpv6", IPPROTO_ICMPV6 },
	{ "sctp", IPPROTO_SCTP },
	{ "default", 0 },
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
	int domain, type, protocol;

	if (argc != 4) {
		fprintf(stderr, "usage: %s domain type protocol\n", argv[0]);
		exit(1);
	}

	domain = lookup_value(argv[1], domains);
	if (domain < 0) {
		fprintf(stderr, "%s: unknown domain %s\n", argv[0], argv[1]);
		exit(1);
	}

	type = lookup_value(argv[2], types);
	if (type < 0) {
		fprintf(stderr, "%s: unknown type %s\n", argv[0], argv[2]);
		exit(1);
	}

	protocol = lookup_value(argv[3], protocols);
	if (protocol < 0) {
		fprintf(stderr, "%s: unknown protocol %s\n", argv[0], argv[3]);
		exit(1);
	}

	sock = socket(domain, type, protocol);
	if (sock < 0) {
		fprintf(stderr, "%s: socket(%s/%d, %s/%d, %s/%d): %s\n",
			argv[0], argv[1], domain, argv[2], type,
			argv[3], protocol, strerror(errno));
		exit(1);
	}
	close(sock);
	exit(0);
}

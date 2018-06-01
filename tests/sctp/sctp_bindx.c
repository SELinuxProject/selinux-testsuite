#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-r] [-v] stream|seq port\n"
		"\nWhere:\n\t"
		"-r      After two bindx ADDs, remove one with bindx REM.\n\t"
		"-v      Print context information.\n\t"
		"        The default is to add IPv4 and IPv6 loopback addrs.\n\t"
		"stream  Use SCTP 1-to-1 style or:\n\t"
		"seq     use SCTP 1-to-Many style.\n\t"
		"port    port.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, type, sock, result;
	struct sockaddr_in ipv4;
	struct sockaddr_in6 ipv6;
	unsigned short port;
	bool rem = false;
	bool verbose = false;
	char *context;

	while ((opt = getopt(argc, argv, "rv")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		case 'r':
			rem = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 2)
		usage(argv[0]);

	if (!strcmp(argv[optind], "stream"))
		type = SOCK_STREAM;
	else if (!strcmp(argv[optind], "seq"))
		type = SOCK_SEQPACKET;
	else
		usage(argv[0]);

	port = atoi(argv[optind + 1]);
	if (!port)
		usage(argv[0]);

	if (verbose) {
		if (getcon(&context) < 0)
			context = strdup("unavailable");
		printf("Process context: %s\n", context);
		free(context);
	}

	sock = socket(PF_INET6, type, IPPROTO_SCTP);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	if (verbose)
		print_context(sock, "Server");

	memset(&ipv4, 0, sizeof(struct sockaddr_in));
	ipv4.sin_family = AF_INET;
	ipv4.sin_port = htons(port);
	ipv4.sin_addr.s_addr = htonl(0x7f000001);

	result = sctp_bindx(sock, (struct sockaddr *)&ipv4, 1,
			    SCTP_BINDX_ADD_ADDR);
	if (result < 0) {
		perror("sctp_bindx ADD - ipv4");
		close(sock);
		exit(1);
	}

	if (verbose)
		printf("sctp_bindx ADD - ipv4\n");

	memset(&ipv6, 0, sizeof(struct sockaddr_in6));
	ipv6.sin6_family = AF_INET6;
	ipv6.sin6_port = htons(port);
	ipv6.sin6_addr = in6addr_loopback;

	result = sctp_bindx(sock, (struct sockaddr *)&ipv6, 1,
			    SCTP_BINDX_ADD_ADDR);
	if (result < 0) {
		perror("sctp_bindx ADD - ipv6");
		close(sock);
		exit(1);
	}

	if (verbose)
		printf("sctp_bindx ADD - ipv6\n");

	if (rem) {
		result = sctp_bindx(sock, (struct sockaddr *)&ipv6, 1,
				    SCTP_BINDX_REM_ADDR);
		if (result < 0) {
			perror("sctp_bindx - REM");
			close(sock);
			exit(1);
		}
		if (verbose)
			printf("sctp_bindx REM - ipv6\n");
	}

	close(sock);
	exit(0);
}

#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-4] [-r] [-v] stream|seq port\n"
		"\nWhere:\n\t"
		"-4      Use two IPv4 loopback addresses.\n\t"
		"-r      After two bindx ADDs, remove one with bindx REM.\n\t"
		"-v      Print context information.\n\t"
		"        The default is to add IPv4 and IPv6 loopback addrs.\n\t"
		"stream  Use SCTP 1-to-1 style or:\n\t"
		"seq     use SCTP 1-to-Many style.\n\t"
		"port    port.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, type, sock, result;
	struct sockaddr_in ipv4, ipv4_extra;
	struct sockaddr *extra_addr;
	struct sockaddr_in6 ipv6;
	unsigned short port;
	bool rem = false, ipv4_only = false;
	bool verbose = false;
	char *context;

	while ((opt = getopt(argc, argv, "4rv")) != -1) {
		switch (opt) {
		case '4':
			ipv4_only = true;
			break;
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

	sock = socket(ipv4_only ? PF_INET : PF_INET6, type, IPPROTO_SCTP);
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
		exit(2);
	}

	if (verbose)
		printf("sctp_bindx ADD - ipv4\n");

	memset(&ipv6, 0, sizeof(struct sockaddr_in6));
	ipv6.sin6_family = AF_INET6;
	ipv6.sin6_port = htons(port);
	ipv6.sin6_addr = in6addr_loopback;

	if (ipv4_only) {
		ipv4_extra = ipv4;
		ipv4_extra.sin_addr.s_addr = htonl(0x7f000002);
		extra_addr = (struct sockaddr *)&ipv4_extra;
	} else {
		extra_addr = (struct sockaddr *)&ipv6;
	}

	result = sctp_bindx(sock, extra_addr, 1,
			    SCTP_BINDX_ADD_ADDR);
	if (result < 0) {
		perror("sctp_bindx ADD - second address");
		close(sock);
		exit(3);
	}

	if (verbose)
		printf("sctp_bindx ADD - %s\n", ipv4_only ? "127.0.0.2" : "::1");

	if (rem) {
		result = sctp_bindx(sock, extra_addr, 1,
				    SCTP_BINDX_REM_ADDR);
		if (result < 0) {
			perror("sctp_bindx - REM");
			close(sock);
			exit(4);
		}
		if (verbose)
			printf("sctp_bindx REM - %s\n", ipv4_only ? "127.0.0.2" : "::1");
	}

	close(sock);
	exit(0);
}

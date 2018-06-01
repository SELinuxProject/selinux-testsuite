#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] stream|seq port\n"
		"\nWhere:\n\t"
		"-v      Print context information.\n\t"
		"stream  Use SCTP 1-to-1 style or:\n\t"
		"seq     use SCTP 1-to-Many style.\n\t"
		"port    Listening port.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, sock, result, on = 1;
	struct addrinfo hints, *res;
	bool verbose = false;
	char *context;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 2)
		usage(argv[0]);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_SCTP;

	if (!strcmp(argv[optind], "stream"))
		hints.ai_socktype = SOCK_STREAM;
	else if (!strcmp(argv[optind], "seq"))
		hints.ai_socktype = SOCK_SEQPACKET;
	else
		usage(argv[0]);

	if (verbose) {
		if (getcon(&context) < 0)
			context = strdup("unavailable");
		printf("Process context: %s\n", context);
		free(context);
	}

	result = getaddrinfo(NULL, argv[optind + 1], &hints, &res);
	if (result < 0) {
		printf("getaddrinfo: %s\n", gai_strerror(result));
		exit(1);
	}

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (result < 0) {
		perror("setsockopt: SO_REUSEADDR");
		close(sock);
		exit(1);
	}

	result = bind(sock, res->ai_addr, res->ai_addrlen);
	if (result < 0) {
		perror("bind");
		close(sock);
		exit(1);
	}

	close(sock);
	exit(0);
}

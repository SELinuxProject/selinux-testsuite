#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] stream|seq addr port\n"
		"\nWhere:\n\t"
		"-v      Print context information.\n\t"
		"stream  Use SCTP 1-to-1 style or:\n\t"
		"seq     use SCTP 1-to-Many style.\n\t"
		"addr    Servers IPv4 or IPv6 address.\n\t"
		"port    port.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, type, srv_sock, client_sock, result, on = 1;
	struct addrinfo srv_hints, client_hints, *srv_res, *client_res;
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

	if ((argc - optind) != 3)
		usage(argv[0]);

	if (!strcmp(argv[optind], "stream"))
		type = SOCK_STREAM;
	else if (!strcmp(argv[optind], "seq"))
		type = SOCK_SEQPACKET;
	else
		usage(argv[0]);

	if (verbose) {
		if (getcon(&context) < 0)
			context = strdup("unavailable");
		printf("Process context: %s\n", context);
		free(context);
	}

	memset(&srv_hints, 0, sizeof(struct addrinfo));
	srv_hints.ai_flags = AI_PASSIVE;
	srv_hints.ai_family = AF_INET6;

	srv_hints.ai_socktype = type;
	srv_hints.ai_protocol = IPPROTO_SCTP;

	/* Set up server side */
	result = getaddrinfo(NULL, argv[optind + 2], &srv_hints, &srv_res);
	if (result < 0) {
		printf("getaddrinfo - server: %s\n", gai_strerror(result));
		exit(1);
	}

	srv_sock = socket(srv_res->ai_family, srv_res->ai_socktype,
			  srv_res->ai_protocol);
	if (srv_sock < 0) {
		perror("socket - server");
		exit(1);
	}

	if (verbose)
		print_context(srv_sock, "Server");

	result = setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &on,
			    sizeof(on));
	if (result < 0) {
		perror("setsockopt: SO_REUSEADDR");
		close(srv_sock);
		exit(1);
	}

	result = bind(srv_sock, srv_res->ai_addr, srv_res->ai_addrlen);
	if (result < 0) {
		perror("bind");
		close(srv_sock);
		exit(1);
	}

	listen(srv_sock, 1);

	/* Set up client side */
	memset(&client_hints, 0, sizeof(struct addrinfo));
	client_hints.ai_socktype = type;
	client_hints.ai_protocol = IPPROTO_SCTP;
	result = getaddrinfo(argv[optind + 1], argv[optind + 2],
			     &client_hints, &client_res);
	if (result < 0) {
		fprintf(stderr, "getaddrinfo - client: %s\n",
			gai_strerror(result));
		exit(1);
	}

	client_sock = socket(client_res->ai_family, client_res->ai_socktype,
			     client_res->ai_protocol);
	if (client_sock < 0) {
		perror("socket - client");
		exit(1);
	}

	if (verbose)
		print_context(client_sock, "Client");

	result = sctp_connectx(client_sock, client_res->ai_addr, 1, NULL);
	if (result < 0) {
		perror("connectx");
		close(srv_sock);
		close(client_sock);
		exit(1);
	}

	close(srv_sock);
	close(client_sock);
	exit(0);
}

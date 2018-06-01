#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] [-o aci|pap|pat] stream|seq addr port\n"
		"\nWhere:\n\t"
		"-v      Print information.\n\t"
		"-o      Test setsockoption(3) using one of the following\n\t"
		"        options:\n\t\t"
		"        aci = SCTP_ASSOCINFO\n\t\t"
		"        pap = SCTP_PEER_ADDR_PARAMS\n\t\t"
		"        pat = SCTP_PEER_ADDR_THLDS\n\t\t"
		"stream  SCTP 1-to-1 style or:\n\t"
		"seq     SCTP 1-to-Many style.\n\t"
		"addr    Servers IPv4 or IPv6 address.\n\t"
		"port    port.\n", progname);
	exit(1);
}

/* Test set_param permission for SCTP_ASSOCINFO */
static void sctp_associnfo(int sk, int option)
{
	int result;
	socklen_t len;
	struct sctp_assocparams assocparams;

	memset(&assocparams, 0, sizeof(struct sctp_assocparams));

	len = sizeof(struct sctp_assocparams);
	result = getsockopt(sk, IPPROTO_SCTP, option, &assocparams, &len);
	if (result < 0) {
		perror("getsockopt: SCTP_ASSOCINFO");
		close(sk);
		exit(1);
	}

	assocparams.sasoc_asocmaxrxt += 5;
	assocparams.sasoc_cookie_life += 15;

	result = setsockopt(sk, IPPROTO_SCTP, option, &assocparams, len);
	if (result < 0) {
		perror("setsockopt: SCTP_ASSOCINFO");
		close(sk);
		exit(1);
	}
}


/* Test set_param permission for SCTP_PEER_ADDR_PARAMS */
static void sctp_peer_addr_params(int sk, int option)
{
	int result;
	struct sctp_paddrparams heartbeat;

	memset(&heartbeat, 0, sizeof(struct sctp_paddrparams));
	heartbeat.spp_flags = SPP_HB_ENABLE;
	heartbeat.spp_hbinterval = 100;
	heartbeat.spp_pathmaxrxt = 1;

	result = setsockopt(sk, IPPROTO_SCTP, option,
			    &heartbeat, sizeof(heartbeat));
	if (result < 0) {
		perror("setsockopt: SCTP_PEER_ADDR_PARAMS");
		close(sk);
		exit(1);
	}
}

int main(int argc, char **argv)
{
	int opt, type, srv_sock, client_sock, result, sockoption = 0;
	struct addrinfo srv_hints, client_hints, *srv_res, *client_res;
	bool verbose = false;
	char *context;

	while ((opt = getopt(argc, argv, "o:v")) != -1) {
		switch (opt) {
		case 'o':
			if (!strcmp(optarg, "aci"))
				sockoption = SCTP_ASSOCINFO;
			else if (!strcmp(optarg, "pap"))
				sockoption = SCTP_PEER_ADDR_PARAMS;
			else if (!strcmp(optarg, "pat")) {
				printf("SCTP_PEER_ADDR_THLDS not currently supported by userspace\n");
				exit(1);
			} else
				usage(argv[0]);
			break;
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

	if (bind(srv_sock, srv_res->ai_addr, srv_res->ai_addrlen) < 0) {
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
		close(client_sock);
		exit(1);
	}

	if (sockoption) {
		switch (sockoption) {
		case SCTP_ASSOCINFO:
			if (verbose)
				printf("Testing: SCTP_ASSOCINFO\n");
			sctp_associnfo(srv_sock, sockoption);
			break;
		case SCTP_PEER_ADDR_PARAMS:
			if (verbose)
				printf("Testing: SCTP_PEER_ADDR_PARAMS\n");
			sctp_peer_addr_params(client_sock, sockoption);
			break;
		}
	} else {

		if (verbose)
			printf("Testing: SCTP_ASSOCINFO\n");
		sctp_associnfo(srv_sock, SCTP_ASSOCINFO);

		if (verbose)
			printf("Testing: SCTP_PEER_ADDR_PARAMS\n");
		sctp_peer_addr_params(client_sock, SCTP_PEER_ADDR_PARAMS);

	}

	close(srv_sock);
	close(client_sock);
	exit(0);
}

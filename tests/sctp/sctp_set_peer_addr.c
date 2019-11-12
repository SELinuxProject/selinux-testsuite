/*
 * This test will allow the server side to add/remove bindx addresses and
 * inform the client side via ASCONF chunks. It will also allow the server
 * side to inform the client that the peer primary address is being updated.
 * The code for checking these parameters are in net/sctp/sm_make_chunk.c
 * sctp_process_asconf_param().
 *
 * To enable the processing of these incoming ASCONF parameters for:
 *      SCTP_PARAM_SET_PRIMARY, SCTP_PARAM_ADD_IP and SCTP_PARAM_DEL_IP
 * the following options must be enabled:
 *	echo 1 > /proc/sys/net/sctp/addip_enable
 *	echo 1 > /proc/sys/net/sctp/addip_noauth_enable
 *
 * If these are not enabled the SCTP_SET_PEER_PRIMARY_ADDR setsockopt
 * fails with EPERM "Operation not permitted", however the bindx calls
 * will complete but the client side will not be informed.
 *
 * NOTES:
 *   1) SCTP_SET_PEER_PRIMARY_ADDR requires a non-loopback IP address.
 *   2) Both addresses MUST be the same type (i.e. IPv4 or IPv6).
 */

#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -v addr new_pri_addr port\n"
		"\nWhere:\n\t"
		"-v           Print status information.\n\t"
		"addr         IPv4/IPv6 address for initial connection.\n\t"
		"new_pri_addr IPv4/IPv6 address that the server will bindx\n\t"
		"             then set to the new SCTP_PRIMARY_ADDR.\n\t"
		"port         port.\n", progname);
	fprintf(stderr,
		"Notes:\n\t"
		"1) addr and new_pri_addr MUST NOT be loopback addresses.\n\t"
		"2) addr and new_pri_addr MUST be same type (IPv4 or IPv6).\n\t"
		"3) IPv6 link-local addresses require the %%<if_name> to\n\t"
		"   obtain scopeid. e.g. fe80::7629:afff:fe0f:8e5d%%wlp6s0\n");
	exit(1);
}

static int peer_count, peer_count_err;

static void getpaddrs_alarm(int sig)
{
	fprintf(stderr, "Get peer address count timer expired - carry on test\n");
	peer_count += 1;
	peer_count_err = true;
}

static void getprimaddr_alarm(int sig)
{
	fprintf(stderr, "Get primary address timer expired - end test.\n");
	exit(1);
}

static void print_primaddr(char *msg, int socket)
{
	int result;
	struct sctp_prim prim;
	struct sockaddr_in *in_addr;
	struct sockaddr_in6 *in6_addr;
	struct sockaddr *paddr;
	socklen_t prim_len;
	char addr_buf[INET6_ADDRSTRLEN];
	const char *addr_ptr = NULL;

	memset(&prim, 0, sizeof(struct sctp_prim));

	prim_len = sizeof(struct sctp_prim);
	result = getsockopt(socket, IPPROTO_SCTP, SCTP_PRIMARY_ADDR,
			    &prim, &prim_len);
	if (result < 0) {
		perror("getsockopt: SCTP_PRIMARY_ADDR");
		exit(1);
	}

	paddr = (struct sockaddr *)&prim.ssp_addr;
	if (paddr->sa_family == AF_INET) {
		in_addr = (struct sockaddr_in *)&prim.ssp_addr;
		addr_ptr = inet_ntop(AF_INET, &in_addr->sin_addr, addr_buf,
				     INET6_ADDRSTRLEN);
	} else if (paddr->sa_family == AF_INET6) {
		in6_addr = (struct sockaddr_in6 *)&prim.ssp_addr;
		addr_ptr = inet_ntop(AF_INET6, &in6_addr->sin6_addr, addr_buf,
				     INET6_ADDRSTRLEN);
	}

	if (!addr_ptr) {
		perror("inet_ntop");
		exit(1);
	}

	printf("%s SCTP_PRIMARY_ADDR: %s\n", msg, addr_ptr);
}

static void get_primaddr(char *addr_buf, int socket)
{
	int result;
	struct sctp_prim prim;
	struct sockaddr_in *in_addr;
	struct sockaddr_in6 *in6_addr;
	struct sockaddr *paddr;
	socklen_t prim_len;
	const char *addr_ptr = NULL;

	memset(&prim, 0, sizeof(struct sctp_prim));
	prim_len = sizeof(struct sctp_prim);
	result = getsockopt(socket, IPPROTO_SCTP, SCTP_PRIMARY_ADDR,
			    &prim, &prim_len);
	if (result < 0) {
		perror("getsockopt: SCTP_PRIMARY_ADDR");
		exit(1);
	}

	paddr = (struct sockaddr *)&prim.ssp_addr;
	if (paddr->sa_family == AF_INET) {
		in_addr = (struct sockaddr_in *)&prim.ssp_addr;
		addr_ptr = inet_ntop(AF_INET, &in_addr->sin_addr, addr_buf,
				     INET6_ADDRSTRLEN);
	} else if (paddr->sa_family == AF_INET6) {
		in6_addr = (struct sockaddr_in6 *)&prim.ssp_addr;
		addr_ptr = inet_ntop(AF_INET6, &in6_addr->sin6_addr, addr_buf,
				     INET6_ADDRSTRLEN);
	}
	if (!addr_ptr) {
		perror("inet_ntop");
		exit(1);
	}
}

int main(int argc, char **argv)
{
	int opt, srv_sock, client_sock, new_sock, result, on = 1;
	struct addrinfo srv_hints, client_hints, *srv_res, *client_res;
	struct addrinfo *new_pri_addr_res;
	struct sockaddr *sa_ptr, *paddrs;
	socklen_t sinlen;
	struct sockaddr_storage sin;
	struct sctp_setpeerprim setpeerprim;
	bool verbose = false, is_ipv6 = false;
	char client_prim_addr[INET6_ADDRSTRLEN];
	char client_prim_new_pri_addr[INET6_ADDRSTRLEN];

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

	if (strchr(argv[optind], ':') && strchr(argv[optind + 1], ':')) {
		is_ipv6 = true;
		srv_hints.ai_family = AF_INET6;
	} else if (strchr(argv[optind], '.') &&
		   strchr(argv[optind + 1], '.')) {
		is_ipv6 = false;
		srv_hints.ai_family = AF_INET;
	} else {
		usage(argv[0]);
	}

	memset(&srv_hints, 0, sizeof(struct addrinfo));
	srv_hints.ai_flags = AI_PASSIVE;
	srv_hints.ai_socktype = SOCK_STREAM;
	srv_hints.ai_protocol = IPPROTO_SCTP;

	/* Set up server side */
	result = getaddrinfo(argv[optind], argv[optind + 2],
			     &srv_hints, &srv_res);
	if (result < 0) {
		fprintf(stderr, "getaddrinfo - server: %s\n",
			gai_strerror(result));
		exit(1);
	}

	result = getaddrinfo(argv[optind], argv[optind + 2],
			     &srv_hints, &srv_res);
	if (result < 0) {
		fprintf(stderr, "getaddrinfo - server: %s\n",
			gai_strerror(result));
		exit(1);
	}
	if (is_ipv6 && verbose)
		printf("Server scopeID: %d\n",
		       ((struct sockaddr_in6 *)
			srv_res->ai_addr)->sin6_scope_id);

	srv_sock = socket(srv_res->ai_family, srv_res->ai_socktype,
			  srv_res->ai_protocol);
	if (srv_sock < 0) {
		perror("socket - server");
		exit(1);
	}

	result = setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR,
			    &on, sizeof(on));
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

	/* Set up client side and connect */
	memset(&client_hints, 0, sizeof(struct addrinfo));
	client_hints.ai_socktype = SOCK_STREAM;
	client_hints.ai_protocol = IPPROTO_SCTP;
	result = getaddrinfo(argv[optind], argv[optind + 2],
			     &client_hints, &client_res);
	if (result < 0) {
		fprintf(stderr, "getaddrinfo - client: %s\n",
			gai_strerror(result));
		close(srv_sock);
		exit(1);
	}
	if (is_ipv6 && verbose)
		printf("Client scopeID: %d\n",
		       ((struct sockaddr_in6 *)
			client_res->ai_addr)->sin6_scope_id);

	client_sock = socket(client_res->ai_family, client_res->ai_socktype,
			     client_res->ai_protocol);
	if (client_sock < 0) {
		perror("socket - client");
		close(srv_sock);
		exit(1);
	}

	result = connect(client_sock, client_res->ai_addr,
			 client_res->ai_addrlen);
	if (result < 0) {
		if (errno != EINPROGRESS)
			perror("connect");
		else
			fprintf(stderr, "connect timeout\n");
		result = 1;
		goto err2;
	}

	/* Obtain address info for the BINDX_ADD and new SCTP_PRIMARY_ADDR. */
	result = getaddrinfo(argv[optind + 1], argv[optind + 2],
			     &client_hints, &new_pri_addr_res);
	if (result < 0) {
		fprintf(stderr, "getaddrinfo - new SCTP_PRIMARY_ADDR: %s\n",
			gai_strerror(result));
		close(srv_sock);
		exit(1);
	}
	if (is_ipv6 && verbose)
		printf("new_pri_addr scopeID: %d\n",
		       ((struct sockaddr_in6 *)
			new_pri_addr_res->ai_addr)->sin6_scope_id);

	/* Get number of peer addresses on CLIENT (should be 1) for a check
	 * later as sctp_bindx SERVER -> CLIENT is non-blocking.
	 */
	peer_count = sctp_getpaddrs(client_sock, 0, &paddrs);
	sctp_freepaddrs(paddrs);
	if (verbose)
		printf("Client peer address count: %d\n", peer_count);

	/* Client and server now set so accept new socket on server side. */
	sinlen = sizeof(sin);
	new_sock = accept(srv_sock, (struct sockaddr *)&sin, &sinlen);
	if (new_sock < 0) {
		perror("accept");
		result = 1;
		goto err2;
	}

	/* Get initial CLIENT primary address (that should be ADDR1). */
	get_primaddr(client_prim_addr, client_sock);

	/* Now call sctp_bindx to add new_pri_addr, this will cause an
	 * ASCONF - SCTP_PARAM_ADD_IP chunk to be sent to the CLIENT.
	 * This is non-blocking so there maybe a delay before the CLIENT
	 * receives the asconf chunk.
	 */
	if (verbose)
		printf("Calling sctp_bindx ADD: %s\n", argv[optind + 1]);

	result = sctp_bindx(new_sock,
			    (struct sockaddr *)new_pri_addr_res->ai_addr,
			    1, SCTP_BINDX_ADD_ADDR);
	if (result < 0) {
		if (errno == EACCES) {
			perror("sctp_bindx ADD");
		} else {
			perror("sctp_bindx ADD");
			result = 1;
			goto err1;
		}
	}
	/* so set an alarm and check number of peer addresses for CLIENT. */
	signal(SIGALRM, getpaddrs_alarm);
	alarm(2);
	peer_count_err = false;
	result = 0;

	while (result != peer_count + 1) {
		result = sctp_getpaddrs(client_sock, 0, &paddrs);
		sctp_freepaddrs(paddrs);

		if (peer_count_err)
			break;
	}
	peer_count = result;

	if (verbose)
		printf("Client peer address count: %d\n", result);

	/* Now that the CLIENT has the new primary address ensure they use
	 * it by SCTP_SET_PEER_PRIMARY_ADDR.
	 */
	memset(&setpeerprim, 0, sizeof(struct sctp_setpeerprim));
	sa_ptr = (struct sockaddr *)&setpeerprim.sspp_addr;
	if (is_ipv6)
		memcpy(sa_ptr, new_pri_addr_res->ai_addr,
		       sizeof(struct sockaddr_in6));
	else
		memcpy(sa_ptr, new_pri_addr_res->ai_addr,
		       sizeof(struct sockaddr_in));

	if (verbose)
		printf("Calling setsockopt SCTP_SET_PEER_PRIMARY_ADDR: %s\n",
		       argv[optind + 1]);

	result = setsockopt(new_sock, IPPROTO_SCTP,
			    SCTP_SET_PEER_PRIMARY_ADDR,
			    &setpeerprim, sizeof(struct sctp_setpeerprim));
	if (result < 0) {
		perror("setsockopt: SCTP_SET_PEER_PRIMARY_ADDR");
		result = 1;
		goto err1;
	}

	/* Now get the new primary address from the client */
	signal(SIGALRM, getprimaddr_alarm);
	alarm(2);
	memcpy(client_prim_new_pri_addr, client_prim_addr, INET6_ADDRSTRLEN);

	while (!strcmp(client_prim_addr, client_prim_new_pri_addr))
		get_primaddr(client_prim_new_pri_addr, client_sock);

	if (verbose) {
		printf("Client initial SCTP_PRIMARY_ADDR: %s\n",
		       client_prim_addr);
		print_primaddr("Server", new_sock);
		printf("Client current SCTP_PRIMARY_ADDR: %s\n",
		       client_prim_new_pri_addr);
	}

	/* Then delete addr1 that checks ASCONF - SCTP_PARAM_DEL_IP. */
	if (verbose)
		printf("Calling sctp_bindx REM: %s\n", argv[optind]);

	result = sctp_bindx(new_sock, (struct sockaddr *)client_res->ai_addr,
			    1, SCTP_BINDX_REM_ADDR);
	if (result < 0) {
		perror("sctp_bindx - REM");
		result = 1;
		goto err1;
	}

	if (!peer_count_err) {
		alarm(2);
		result = 0;

		while (result != peer_count - 1) {
			result = sctp_getpaddrs(client_sock, 0, &paddrs);
			sctp_freepaddrs(paddrs);
		}

		if (verbose)
			printf("Client peer address count: %d\n", result);
	}

	/* Compare the client primary addresses, they should be different. */
	if (!strcmp(client_prim_addr, client_prim_new_pri_addr)) {
		fprintf(stderr,
			"Client addr: %s same as new_pri_addr: %s - SCTP_SET_PEER_PRIMARY_ADDR failed\n",
			client_prim_addr, client_prim_new_pri_addr);
		result = 1;
		goto err1;
	}

	if (verbose)
		printf("Client primary address changed successfully.\n");

	result = 0;

err1:
	close(new_sock);
err2:
	close(srv_sock);
	close(client_sock);
	exit(result);
}

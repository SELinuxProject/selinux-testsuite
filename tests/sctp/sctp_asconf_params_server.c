#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-f file] [-v] addr new_pri_addr port\n"
		"\nWhere:\n\t"
		"-f           Write a line to the file when listening starts.\n\t"
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

int main(int argc, char **argv)
{
	int opt, srv_sock, new_sock, result, on = 1;
	struct addrinfo srv_hints, *srv_res;
	struct addrinfo *new_pri_addr_res;
	struct sockaddr *sa_ptr;
	socklen_t sinlen;
	struct sockaddr_storage sin;
	struct sctp_setpeerprim setpeerprim;
	bool verbose = false, is_ipv6 = false;
	char buffer[128];
	char *flag_file = NULL;

	while ((opt = getopt(argc, argv, "f:v")) != -1) {
		switch (opt) {
		case 'f':
			flag_file = optarg;
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

	if (flag_file) {
		FILE *f = fopen(flag_file, "w");
		if (!f) {
			perror("Flag file open");
			exit(1);
		}
		fprintf(f, "listening\n");
		fclose(f);
	}

	new_sock = accept(srv_sock, (struct sockaddr *)&sin, &sinlen);
	if (new_sock < 0) {
		perror("accept");
		result = 1;
		goto err2;
	}

	/* This waits for a client message before continuing. */
	result = read(new_sock, &buffer, sizeof(buffer));
	if (result < 0) {
		perror("read");
		exit(1);
	}
	buffer[result] = 0;
	if (verbose)
		printf("%s\n", buffer);

	/* Obtain address info for the BINDX_ADD and new SCTP_PRIMARY_ADDR. */
	result = getaddrinfo(argv[optind + 1], argv[optind + 2],
			     &srv_hints, &new_pri_addr_res);
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


	/* Now call sctp_bindx to add ADDR2, this will cause an
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

	/* This waits for a client message before continuing. */
	result = read(new_sock, &buffer, sizeof(buffer));
	if (result < 0) {
		perror("read");
		exit(1);
	}
	buffer[result] = 0;
	if (verbose)
		printf("%s\n", buffer);

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
	/* Sleep a sec to ensure client get info. */
	result = read(new_sock, &buffer, sizeof(buffer));
	if (result < 0) {
		perror("read");
		exit(1);
	}
	buffer[result] = 0;
	if (verbose)
		printf("%s\n", buffer);

	/* Then delete addr that checks ASCONF - SCTP_PARAM_DEL_IP. */
	if (verbose)
		printf("Calling sctp_bindx REM: %s\n", argv[optind]);

	result = sctp_bindx(new_sock, (struct sockaddr *)srv_res->ai_addr,
			    1, SCTP_BINDX_REM_ADDR);
	if (result < 0) {
		perror("sctp_bindx - REM");
		result = 1;
		goto err1;
	}

	result = read(new_sock, &buffer, sizeof(buffer));
	if (result <= 0) {
		if (errno != 0)
			perror("read");
		result = 1;
		goto err1;
	}
	buffer[result] = 0;
	if (verbose)
		printf("%s\n", buffer);

	result = read(new_sock, &buffer, sizeof(buffer));
	if (result < 0) {
		perror("read");
		exit(1);
	}
	buffer[result] = 0;
	if (verbose)
		printf("%s\n", buffer);

	result = 0;

err1:
	close(new_sock);
err2:
	close(srv_sock);
	exit(result);
}

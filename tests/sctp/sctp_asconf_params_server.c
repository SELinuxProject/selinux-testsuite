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
	int opt, srv_sock, result, on = 1, flags = 0, if_index = 0;
	size_t new_len;
	struct addrinfo srv_hints, *srv_res;
	struct addrinfo *new_pri_addr_res;
	struct sockaddr *sa_ptr;
	struct sockaddr_storage sin;
	socklen_t sinlen = sizeof(sin);
	struct sctp_setpeerprim setpeerprim;
	struct sctp_sndrcvinfo sinfo;
	bool verbose = false, is_ipv6 = false;
	char buffer[512];
	char *flag_file = NULL, *ptr, if_name[30], *new_pri_addr = NULL;

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
	srv_hints.ai_socktype = SOCK_SEQPACKET;
	srv_hints.ai_protocol = IPPROTO_SCTP;

	/*
	 * Setup the 2nd address for sending to client.
	 */
	/* If local link, get if_name & if_index */
	if (is_ipv6) {
		ptr = strpbrk(argv[optind], "%");
		if (ptr) {
			strcpy(if_name, ptr + 1);
			if_index = if_nametoindex(if_name);
			if (!if_index) {
				perror("Server if_nametoindex");
				exit(1);
			}
			if (verbose)
				printf("if_name: %s if_index: %d\n",
				       if_name, if_index);
		}
	}

	/* Now remove % if on new peer */
	new_len = strcspn(argv[optind + 1], "%");
	new_pri_addr = strndup(argv[optind + 1], new_len);

	/*
	 * Use the 1st address for server side setup.
	 */
	result = getaddrinfo(argv[optind], argv[optind + 2],
			     &srv_hints, &srv_res);
	if (result < 0) {
		fprintf(stderr, "Server getaddrinfo err: %s\n",
			gai_strerror(result));
		exit(1);
	}
	if (is_ipv6 && verbose)
		printf("Server address: %s has scopeID: %d\n", argv[optind],
		       ((struct sockaddr_in6 *)
			srv_res->ai_addr)->sin6_scope_id);

	srv_sock = socket(srv_res->ai_family, srv_res->ai_socktype,
			  srv_res->ai_protocol);
	if (srv_sock < 0) {
		perror("Server socket");
		freeaddrinfo(srv_res);
		exit(1);
	}

	result = setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &on,
			    sizeof(on));
	if (result < 0) {
		perror("Server setsockopt: SO_REUSEADDR");
		goto err1;
	}

	result = sctp_bindx(srv_sock, srv_res->ai_addr, 1, SCTP_BINDX_ADD_ADDR);
	if (result < 0) {
		perror("Server bind");
		goto err1;
	}

	listen(srv_sock, SOMAXCONN);

	if (flag_file) {
		FILE *f = fopen(flag_file, "w");
		if (!f) {
			perror("Server Flag file open");
			goto err1;
		}
		fprintf(f, "listening\n");
		fclose(f);
	}

	result = set_subscr_events(srv_sock, on, on, on, on);
	if (result < 0) {
		perror("Server setsockopt SCTP_EVENTS");
		goto err1;
	}

	/*
	 * Receive notifications for initial addr changes, then a request for
	 * the 'new_pri_addr' from the client.
	 */
	memset(&sinfo, 0, sizeof(struct sctp_sndrcvinfo));
	while (1) {
		result = sctp_recvmsg(srv_sock, buffer, sizeof(buffer),
				      (struct sockaddr *)&sin, &sinlen,
				      &sinfo, &flags);
		if (result < 0) {
			perror("Server sctp_recvmsg-1");
			goto err1;
		}

		if (flags & MSG_NOTIFICATION && flags & MSG_EOR) {
			result = handle_event(buffer, NULL, NULL, verbose,
					      "Server");
			if (result == EVENT_SHUTDOWN)
				goto err1;
		} else {
			if (verbose)
				printf("Server received: %s\n", buffer);
			break;
		}
	}

	/*
	 * Request the peer sets the 1st cmd line address as peer primary.
	 * This uses Dynamic Address Reconfiguration by sending an asconf
	 * chunk with SCTP_PARAM_SET_PRIMARY set to the client.
	 */
	memset(&setpeerprim, 0, sizeof(struct sctp_setpeerprim));
	setpeerprim.sspp_assoc_id = sinfo.sinfo_assoc_id;
	sa_ptr = (struct sockaddr *)&setpeerprim.sspp_addr;
	if (is_ipv6)
		memcpy(sa_ptr, srv_res->ai_addr,
		       sizeof(struct sockaddr_in6));
	else
		memcpy(sa_ptr, srv_res->ai_addr,
		       sizeof(struct sockaddr_in));

	result = setsockopt(srv_sock, IPPROTO_SCTP,
			    SCTP_SET_PEER_PRIMARY_ADDR,
			    &setpeerprim,
			    sizeof(struct sctp_setpeerprim));
	if (result < 0) {
		perror("Server setsockopt: SCTP_SET_PEER_PRIMARY_ADDR");
		goto err1;
	}
	if (verbose)
		printf("Server setsockopt: SCTP_SET_PEER_PRIMARY_ADDR with:\n\t%s\n",
		       argv[optind + 1]);

	if (sin.ss_family == AF_INET6) /* Set scope_id for local link */
		((struct sockaddr_in6 *)&sin)->sin6_scope_id = if_index;

	/* Send client the new primary address */
	result = sctp_sendmsg(srv_sock, new_pri_addr, new_len,
			      (struct sockaddr *)&sin,
			      sinlen, 0, 0, 0, 0, 0);
	free(new_pri_addr);
	if (result < 0) {
		perror("Server sctp_sendmsg");
		goto err1;
	}

	/* Ready the 2nd cmd line address for BINDX_ADD */
	result = getaddrinfo(argv[optind + 1], argv[optind + 2],
			     &srv_hints, &new_pri_addr_res);
	if (result < 0) {
		fprintf(stderr, "Server getaddrinfo - new SCTP_PRIMARY_ADDR: %s\n",
			gai_strerror(result));
		goto err1;
	}

	if (is_ipv6 && verbose)
		printf("Server new_pri_addr: %s has scopeID: %d\n",
		       argv[optind + 1],
		       ((struct sockaddr_in6 *)
			new_pri_addr_res->ai_addr)->sin6_scope_id);

	/*
	 * Now call sctp_bindx(3) to add 'new_pri_addr'. This uses Dynamic
	 * Address Reconfiguration by sending an asconf chunk with
	 * SCTP_PARAM_ADD_IP set to the client.
	 */
	result = sctp_bindx(srv_sock,
			    (struct sockaddr *)new_pri_addr_res->ai_addr,
			    1, SCTP_BINDX_ADD_ADDR);
	if (result < 0) {
		perror("Server sctp_bindx ADD");
		goto err1;
	}
	if (verbose)
		printf("Server sctp_bindx(3) SCTP_BINDX_ADD_ADDR:\n\t%s\n",
		       argv[optind + 1]);

	/* Then delete 'addr' that DAR's - SCTP_PARAM_DEL_IP. */
	result = sctp_bindx(srv_sock, (struct sockaddr *)srv_res->ai_addr,
			    1, SCTP_BINDX_REM_ADDR);
	if (result < 0) {
		perror("Server sctp_bindx - REM");
		goto err1;
	}
	if (verbose)
		printf("Server sctp_bindx(3) SCTP_BINDX_REM_ADDR:\n\t%s\n",
		       argv[optind]);

	/*
	 * End of test - The 'new_pri_addr' is not used for any sessions as
	 * the objective was to only exercise Dynamic Address Reconfiguration
	 * so wait for client to complete before closing otherwise it
	 * errors with ENOTCONN
	 */
	while (1) {
		result = sctp_recvmsg(srv_sock, buffer, sizeof(buffer),
				      (struct sockaddr *)&sin, &sinlen,
				      &sinfo, &flags);
		if (result < 0) {
			perror("Server sctp_recvmsg-2");
			goto err1;
		}

		if (verbose)
			printf("Client assoc_id: %d\n", sinfo.sinfo_assoc_id);

		if (flags & MSG_NOTIFICATION) {
			result = handle_event(buffer, NULL, NULL, verbose, "Server");
			if (result == EVENT_SHUTDOWN)
				break;
		} else {
			if (verbose)
				printf("Server received: %s\n", buffer);
			break;
		}
	}

	result = 0;
end:
	close(srv_sock);
	freeaddrinfo(srv_res);
	freeaddrinfo(new_pri_addr_res);
	return result;
err1:
	result = -1;
	goto end;
}

/* This test will allow the server side to add/remove bindx addresses and
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
 *   2) Both addresses used by the client/server MUST be the same type
 *      (i.e. IPv4 or IPv6).
 *   3) The iptables default for Fedora does not allow SCTP remote traffic.
 *      To allow this set the following:
 *          iptables -I INPUT 1 -p sctp -j ACCEPT
 *          ip6tables -I INPUT 1 -p sctp -j ACCEPT
 */

#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] addr port\n"
		"\nWhere:\n\t"
		"-v     Print status information.\n\t"
		"addr   IPv4 or IPv6 address (MUST NOT be loopback address).\n\t"
		"port   port.\n", progname);

	fprintf(stderr,
		"Notes:\n\t"
		"1) addr and the server side new_pri_addr address MUST be\n\t"
		"   same type (IPv4 or IPv6).\n\t"
		"2) IPv6 link-local addresses require the %%<if_name> to\n\t"
		"   obtain scopeid. e.g. fe80::7629:afff:fe0f:8e5d%%wlp6s0\n");
	exit(1);
}

static int get_set_primaddr(int socket, sctp_assoc_t id, bool verbose)
{
	int result;
	struct sctp_prim prim;	/* Defined in linux/sctp.h */
	socklen_t prim_len;

	/*
	 * At this point the new primary address is already set. To test the
	 * bind permission, just reset the address.
	 */

	memset(&prim, 0, sizeof(struct sctp_prim));
	prim_len = sizeof(struct sctp_prim);
	prim.ssp_assoc_id = id;

	result = getsockopt(socket, IPPROTO_SCTP, SCTP_PRIMARY_ADDR,
			    &prim, &prim_len);
	if (result < 0) {
		perror("Client getsockopt: SCTP_PRIMARY_ADDR");
		return 50;
	}

	if (verbose)
		print_addr_info((struct sockaddr *)&prim.ssp_addr,
				"Client getsockopt - SCTP_PRIMARY_ADDR:");

	/*
	 * If the new primary address is an IPv6 local link address, it will
	 * have been received by the DAR process with a scope id of '0'.
	 * Therefore when the setsockopt is called it will error with EINVAL.
	 * To resolve this set scope_id=1 (first link) before the call.
	 */
	struct sockaddr_in6 *addr6;
	struct sockaddr *sin;

	sin = (struct sockaddr *)&prim.ssp_addr;

	if (sin->sa_family == AF_INET6) {
		addr6 = (struct sockaddr_in6 *)sin;
		if (IN6_IS_ADDR_LINKLOCAL(&addr6->sin6_addr) &&
		    ((struct sockaddr_in6 *)addr6)->sin6_scope_id == 0) {
			((struct sockaddr_in6 *)addr6)->sin6_scope_id = 1;
			if (verbose)
				printf("Client set new Local Link primary address scope_id=1\n");
		}
	}

	/*
	 * This tests the net/sctp/socket.c sctp_setsockopt_primary_addr()
	 * SCTP_PRIMARY_ADDR function by setting policy to:
	 *   allow sctp_asconf_params_client_t self:sctp_socket { bind };
	 * or:
	 *   neverallow sctp_asconf_params_client_t self:sctp_socket { bind };
	 */
	result = setsockopt(socket, IPPROTO_SCTP, SCTP_PRIMARY_ADDR,
			    &prim, prim_len);
	if (result < 0) {
		perror("Client setsockopt: SCTP_PRIMARY_ADDR");
		return 51;
	}
	if (verbose)
		print_addr_info((struct sockaddr *)&prim.ssp_addr,
				"Client set SCTP_PRIMARY_ADDR to:");
	return 0;
}

int main(int argc, char **argv)
{
	int opt, client_sock, result, flags = 0, on = 1;
	struct addrinfo client_hints, *client_res;
	struct sctp_sndrcvinfo sinfo;
	struct sockaddr_storage sin;
	socklen_t sinlen = sizeof(sin);
	struct timeval tm;
	bool verbose = false;
	char buffer[512];
	char msg[] = "Send peer address";
	char *rcv_new_addr_buf = NULL;

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

	memset(&client_hints, 0, sizeof(struct addrinfo));
	client_hints.ai_socktype = SOCK_SEQPACKET;
	client_hints.ai_protocol = IPPROTO_SCTP;
	result = getaddrinfo(argv[optind], argv[optind + 1],
			     &client_hints, &client_res);
	if (result < 0) {
		fprintf(stderr, "Client getaddrinfo err: %s\n",
			gai_strerror(result));
		exit(1);
	}

	client_sock = socket(client_res->ai_family, client_res->ai_socktype,
			     client_res->ai_protocol);
	if (client_sock < 0) {
		perror("Client socket");
		freeaddrinfo(client_res);
		exit(1);
	}

	/* Need to set a timeout if no reply from server */
	memset(&tm, 0, sizeof(struct timeval));
	tm.tv_sec = 3;
	result = setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tm, sizeof(tm));
	if (result < 0) {
		perror("Client setsockopt: SO_RCVTIMEO");
		goto err1;
	}

	result = setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, &tm, sizeof(tm));
	if (result < 0) {
		perror("Client setsockopt: SO_SNDVTIMEO");
		goto err1;
	}

	result = connect(client_sock, client_res->ai_addr,
			 client_res->ai_addrlen);
	if (result < 0) {
		perror("Client connect");
		goto err1;
	}

	result = set_subscr_events(client_sock, on, on, on, on);
	if (result < 0) {
		perror("Client setsockopt SCTP_EVENTS");
		goto err1;
	}

	/* Send msg to form an association with server */
	result = sctp_sendmsg(client_sock, msg, sizeof(msg),
			      client_res->ai_addr,
			      client_res->ai_addrlen,
			      0, 0, 0, 0, 0);
	if (result < 0) {
		perror("Client sctp_sendmsg-1");
		goto err1;
	}

	rcv_new_addr_buf = NULL;
	memset(&sinfo, 0, sizeof(struct sctp_sndrcvinfo));
	/*
	 * Should receive notifications for initial addr change, then
	 * the address to match against from the server, then change to new
	 * peer addr and finally exit.
	 */
	while (1) {
		memset(buffer, 0, sizeof(buffer));

		result = sctp_recvmsg(client_sock, buffer,
				      sizeof(buffer),
				      (struct sockaddr *)&sin,
				      &sinlen, &sinfo, &flags);
		if (result < 0 && errno == EAGAIN) {
			result = EAGAIN;
			fprintf(stderr, "Client error 'Dynamic Address Reconfiguration'\n");
			goto end;
		} else if (result < 0) {
			perror("Client sctp_recvmsg-1");
			goto err1;
		}

		if (sinfo.sinfo_assoc_id) {
			if (verbose)
				printf("Client assoc_id: %d\n",
				       sinfo.sinfo_assoc_id);
		}
		if (flags & MSG_NOTIFICATION && flags & MSG_EOR) {
			result = handle_event(buffer, rcv_new_addr_buf,
					      NULL, verbose, "Client");
			if (result == EVENT_ADDR_MATCH) /* Have new primary addr */
				break;
		} else { /* Should receive only one buffer from server */
			if (verbose)
				printf("Client received new pri addr: %s\n",
				       buffer);

			rcv_new_addr_buf = strdup(buffer);
		}
	}

	/* Get new CLIENT primary address */
	result = get_set_primaddr(client_sock, sinfo.sinfo_assoc_id, verbose);
	if (result > 0)
		goto end;

	result = 0;
end:
	close(client_sock);
	freeaddrinfo(client_res);
	free(rcv_new_addr_buf);
	return result;
err1:
	result = -1;
	goto end;
}

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
		"usage:  %s [-v] [-n] addr port\n"
		"\nWhere:\n\t"
		"-v     Print status information.\n\t"
		"-n     No bindx_rem will be received from server. This happens\n\t"
		"       when the client and server are on different systems.\n\t"
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

static int peer_count, peer_count_err;
static void getpaddrs_alarm(int sig)
{
	fprintf(stderr,
		"Get peer address count timer expired - carry on test\n");
	peer_count += 1;
	peer_count_err = true;
}

static void getprimaddr_alarm(int sig)
{
	fprintf(stderr, "Get primary address timer expired - end test.\n");
	exit(1);
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
	int opt, client_sock, result, len;
	struct addrinfo client_hints, *client_res;
	struct sockaddr *paddrs;
	bool verbose = false, no_bindx_rem = false;
	char client_prim_addr1[INET6_ADDRSTRLEN];
	char client_prim_addr2[INET6_ADDRSTRLEN];
	char buffer[1024];

	while ((opt = getopt(argc, argv, "vn")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		case 'n':
			no_bindx_rem = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 2)
		usage(argv[0]);

	/* Set up client side and connect */
	memset(&client_hints, 0, sizeof(struct addrinfo));
	client_hints.ai_socktype = SOCK_STREAM;
	client_hints.ai_protocol = IPPROTO_SCTP;
	result = getaddrinfo(argv[optind], argv[optind + 1],
			     &client_hints, &client_res);
	if (result < 0) {
		fprintf(stderr, "getaddrinfo - client: %s\n",
			gai_strerror(result));
		exit(1);
	}


	/* printf("Client scopeID: %d\n",
	 *        ((struct sockaddr_in6 *)client_res->ai_addr)->sin6_scope_id);
	 */

	client_sock = socket(client_res->ai_family, client_res->ai_socktype,
			     client_res->ai_protocol);
	if (client_sock < 0) {
		perror("socket");
		exit(1);
	}

	result = connect(client_sock, client_res->ai_addr,
			 client_res->ai_addrlen);
	if (result < 0) {
		if (errno != EINPROGRESS)
			perror("connect");
		else
			fprintf(stderr, "connect timeout\n");

		close(client_sock);
		exit(1);
	}

	/* Get number of peer addresses on CLIENT (should be 1) for a check
	 * later as sctp_bindx SERVER -> CLIENT is non-blocking.
	 */
	peer_count = sctp_getpaddrs(client_sock, 0, &paddrs);
	sctp_freepaddrs(paddrs);
	len = sprintf(buffer, "Client peer address count: %d", peer_count);
	if (verbose)
		printf("%s\n", buffer);


	/* Get initial CLIENT primary address (that should be ADDR1). */
	get_primaddr(client_prim_addr1, client_sock);

	/* server waiting for write before sending BINDX_ADD */
	result = write(client_sock, buffer, len);
	if (result < 0) {
		perror("write");
		close(client_sock);
		exit(1);
	}

	/* Sleep a while as server pings us the new address */
	sleep(1);
	/* then set an alarm and check number of peer addresses for CLIENT */
	signal(SIGALRM, getpaddrs_alarm);
	alarm(2);
	peer_count_err = false;
	result = 0;

	while (result != peer_count + 1) {
		result = sctp_getpaddrs(client_sock, 0, &paddrs);
		if (result > 0)
			sctp_freepaddrs(paddrs);

		if (peer_count_err)
			break;
	}
	alarm(0);
	peer_count = result;

	len = sprintf(buffer, "Client peer address count: %d", result);
	if (verbose)
		printf("%s\n", buffer);

	/* server waiting for write before send SCTP_SET_PEER_PRIMARY_ADDR */
	result = write(client_sock, buffer, len);
	if (result < 0) {
		perror("write");
		close(client_sock);
		exit(1);
	}

	/* Now get the new primary address from the client */
	signal(SIGALRM, getprimaddr_alarm);
	alarm(2);
	memcpy(client_prim_addr2, client_prim_addr1, INET6_ADDRSTRLEN);

	while (!strcmp(client_prim_addr1, client_prim_addr2))
		get_primaddr(client_prim_addr2, client_sock);

	alarm(0);
	len = sprintf(buffer,
		      "Client initial SCTP_PRIMARY_ADDR: %s\nClient current SCTP_PRIMARY_ADDR: %s",
		      client_prim_addr1, client_prim_addr2);
	if (verbose)
		printf("%s\n", buffer);

	if (!no_bindx_rem) {
		/* Let server send bindx_rem */
		result = write(client_sock, buffer, len);
		if (result < 0) {
			perror("write");
			close(client_sock);
			exit(1);
		}

		/* Then delete addr that checks ASCONF - SCTP_PARAM_DEL_IP */
		if (!peer_count_err) {
			signal(SIGALRM, getprimaddr_alarm);
			alarm(2);
			result = 0;
			while (result != peer_count - 1) {
				result = sctp_getpaddrs(client_sock,
							0, &paddrs);
				if (result > 0)
					sctp_freepaddrs(paddrs);

				if (peer_count_err)
					break;
			}
			alarm(0);
			sprintf(buffer, "Client peer address count: %d",
				result);
			if (verbose)
				printf("%s\n", buffer);
		}
	}

	/* server waiting for client peer address count */
	result = write(client_sock, buffer, len);
	if (result < 0) {
		perror("write");
		close(client_sock);
		exit(1);
	}

	/* Compare the client primary addresses, they should be different. */
	if (!strcmp(client_prim_addr1, client_prim_addr2)) {
		len = sprintf(buffer,
			      "Client ADDR1: %s same as ADDR2: %s - SCTP_SET_PEER_PRIMARY_ADDR failed",
			      client_prim_addr1, client_prim_addr2);
		fprintf(stderr, "%s\n", buffer);

		/* server waiting for write to finish */
		result = write(client_sock, buffer, len);
		if (result < 0) {
			perror("write");
			close(client_sock);
		}
		exit(1);
	}

	len = sprintf(buffer, "Client primary address changed successfully\n");
	if (verbose)
		printf("%s\n", buffer);

	/* server waiting for write to finish */
	result = write(client_sock, buffer, len);
	if (result < 0) {
		perror("write");
		close(client_sock);
		exit(1);
	}

	close(client_sock);
	exit(0);
}

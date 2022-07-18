#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-e expected_msg] [-v] [-n] [-x] addr port\n"
		"\nWhere:\n\t"

		"-e      Optional expected message from server e.g. \"nopeer\".\n\t"
		"        If not present the client context will be used as a\n\t"
		"        comparison with the servers reply.\n\t"
		"-n      Do NOT call connect(3) or connectx(3).\n\t"
		"-v      Print context and ip options information.\n\t"
		"-x      Use sctp_connectx(3) instead of connect(3).\n\t"
		"addr    IPv4 or IPv6 address (e.g. 127.0.0.1 or ::1).\n\t"
		"port    Port for accessing server.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, sock, result, save_errno, peeloff_sk = 0, flags;
	int on = 1, off = 0;
	sctp_assoc_t assoc_id = 0;
	socklen_t sinlen;
	struct sockaddr_storage sin;
	struct addrinfo hints, *serverinfo;
	char byte = 0x41, label[1024], *expected = NULL;
	bool verbose = false, connectx = false, no_connects = false;
	bool ipv4 = false;
	bool expected_flg = false;
	char *context;
	struct timeval tm;

	while ((opt = getopt(argc, argv, "e:vxmn")) != -1) {
		switch (opt) {
		case 'e':
			expected_flg = true;
			expected = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		case 'n':
			no_connects = true;
			break;
		case 'x':
			connectx = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 2)
		usage(argv[0]);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_protocol = IPPROTO_SCTP;
	hints.ai_socktype = SOCK_SEQPACKET;
	if (verbose) {
		if (getcon(&context) < 0)
			context = strdup("unavailable");
		printf("Client process context: %s\n", context);
		free(context);
	}

	result = getaddrinfo(argv[optind], argv[optind + 1], &hints,
			     &serverinfo);
	if (result < 0) {
		fprintf(stderr, "Client getaddrinfo: %s\n",
			gai_strerror(result));
		exit(2);
	}

	if (serverinfo->ai_family == AF_INET)
		ipv4 = true;

	sock = socket(serverinfo->ai_family, serverinfo->ai_socktype,
		      serverinfo->ai_protocol);
	if (sock < 0) {
		perror("Client socket");
		exit(3);
	}

	/*
	 * These timeouts are set to test whether the peer { recv } completes
	 * or not when the permission is denied.
	 */
	tm.tv_sec = 4;
	tm.tv_usec = 0;
	result = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tm, sizeof(tm));
	if (result < 0) {
		perror("Client setsockopt: SO_SNDTIMEO");
		exit(4);
	}

	result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tm, sizeof(tm));
	if (result < 0) {
		perror("Client setsockopt: SO_RCVTIMEO");
		exit(5);
	}

	/* Subscribe to assoc_id events */
	result = set_subscr_events(sock, off, on, off, off);
	if (result < 0) {
		perror("Client setsockopt: SCTP_EVENTS");
		close(sock);
		exit(1);
	}

	if (!no_connects) {
		if (connectx)
			result = sctp_connectx(sock, serverinfo->ai_addr, 1, NULL);
		else
			result = connect(sock, serverinfo->ai_addr,
					 serverinfo->ai_addrlen);
		if (result < 0) {
			save_errno = errno;
			close(sock);
			perror("Client connect");
			switch (save_errno) {
			case EINPROGRESS:
				exit(6);
				break;
			case ENOSPC:
				exit(7);
				break;
			case EACCES:
				exit(8);
				break;
			default:
				exit(9);
			}
		}
		if (verbose) {
			print_context(sock, "Client connect");
			print_ip_option(sock, ipv4, "Client connect");
		}
	} else {
		/* First send a message to get an association. */
		result = sctp_sendmsg(sock, &byte, 1,
				      serverinfo->ai_addr,
				      serverinfo->ai_addrlen,
				      0, 0, 0, 0, 0);
		if (result < 0) {
			perror("Client sctp_sendmsg");
			close(sock);
			exit(12);
		}

		if (verbose) {
			print_context(sock, "Client SEQPACKET sctp_sendmsg");
			print_ip_option(sock, ipv4,
					"Client SEQPACKET sctp_sendmsg");
		}
	}

	/* Get assoc_id for sctp_peeloff() */
	sinlen = sizeof(sin);
	flags = 0;
	result = sctp_recvmsg(sock, label, sizeof(label),
			      (struct sockaddr *)&sin, &sinlen,
			      NULL, &flags);
	if (result < 0) {
		perror("Client sctp_recvmsg-1");
		close(sock);
		exit(1);
	}

	if ((flags & (MSG_NOTIFICATION | MSG_EOR)) != (MSG_NOTIFICATION | MSG_EOR)) {
		printf("Invalid sctp_recvmsg response FLAGS: %x\n", flags);
		close(sock);
		exit(1);
	}
	handle_event(label, NULL, &assoc_id, verbose, "Peeloff Client");
	if (assoc_id <= 0) {
		printf("Client Invalid association ID: %d\n", assoc_id);
		close(sock);
		exit(1);
	}
	/* No more notifications */
	result = set_subscr_events(sock, off, off, off, off);
	if (result < 0) {
		perror("Client setsockopt: SCTP_EVENTS");
		close(sock);
		exit(1);
	}

	peeloff_sk = sctp_peeloff(sock, assoc_id);
	if (peeloff_sk < 0) {
		perror("Client sctp_peeloff");
		close(sock);
		exit(1);
	}
	if (verbose) {
		printf("Client sctp_peeloff(3) on sk: %d with association ID: %d\n",
		       peeloff_sk, assoc_id);
		print_context(peeloff_sk, "Client PEELOFF");
	}

	if (!no_connects) {
		result = sctp_sendmsg(peeloff_sk, &byte, 1,
				      (struct sockaddr *)&sin, sinlen,
				      0, 0, 0, 0, 0);
		if (result < 0) {
			perror("Client sctp_sendmsg");
			close(peeloff_sk);
			close(sock);
			exit(12);
		}

		if (verbose) {
			print_context(peeloff_sk,
				      "Client SEQPACKET peeloff sctp_sendmsg");
			print_ip_option(peeloff_sk, ipv4,
					"Client SEQPACKET peeloff sctp_sendmsg");
		}
	}

	result = sctp_recvmsg(peeloff_sk, label, sizeof(label),
			      NULL, 0, NULL, NULL);
	if (result < 0) {
		perror("Client sctp_recvmsg");
		close(peeloff_sk);
		close(sock);
		exit(13);
	}

	label[result] = 0;
	close(peeloff_sk);
	close(sock);

	if (!expected) {
		result = getcon(&expected);
		if (result < 0) {
			perror("Client getcon");
			exit(14);
		}
	}

	if (!expected_flg && cmp_context_mls(expected, label)) {
		fprintf(stderr, "Client expected %s, got %s\n",
			expected, label);
		exit(15);
	} else if (expected_flg && cmp_context_type_mls(expected, label)) {
		fprintf(stderr, "Client expected %s, got %s\n",
			expected, label);
		exit(15);
	} else if (verbose) {
		printf("Client received %s\n", label);
	}

	exit(0);
}

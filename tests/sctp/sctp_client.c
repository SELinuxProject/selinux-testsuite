#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-e expected_msg] [-v] [-n] [-x] stream|seq addr port\n"
		"\nWhere:\n\t"

		"-e      Optional expected message from server e.g. \"nopeer\".\n\t"
		"        If not present the client context will be used as a\n\t"
		"        comparison with the servers reply.\n\t"
		"-n      Do NOT call connect(3) or connectx(3).\n\t"
		"-v      Print context and ip options information.\n\t"
		"-x      Use sctp_connectx(3) instead of connect(3).\n\t"
		"stream  Use SCTP 1-to-1 style or:\n\t"
		"seq     use SCTP 1-to-Many style.\n\t"
		"addr    IPv4 or IPv6 address (e.g. 127.0.0.1 or ::1).\n\t"
		"port    Port for accessing server.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, sock, result, save_errno;
	socklen_t opt_len;
	struct addrinfo hints, *serverinfo;
	char byte = 0x41, label[1024], *expected = NULL;
	bool verbose = false, connectx = false, no_connects = false;
	bool ipv4 = false, expect_ipopt = false;
	char *context;
	struct timeval tm;

	while ((opt = getopt(argc, argv, "e:vxmni")) != -1) {
		switch (opt) {
		case 'e':
			expected = optarg;
			break;
		case 'i':
			expect_ipopt = true;
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

	if ((argc - optind) != 3)
		usage(argv[0]);

	memset(&hints, 0, sizeof(struct addrinfo));
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
		printf("Client process context: %s\n", context);
		free(context);
	}

	result = getaddrinfo(argv[optind + 1], argv[optind + 2], &hints,
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
	 * or not when the permission is denied. These errors will be
	 * returned during testing:
	 *    EINPROGRESS - Operation now in progress - SOCK_STREAM
	 *        Uses SO_SNDTIMEO when using connect(2) or sctp_connectx(3)
	 *    EAGAIN - Resource temporarily unavailable - SOCK_SEQPACKET
	 *        Uses SO_RCVTIMEO when NO connects are called.
	 */
	tm.tv_sec = 2;
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

	if (!no_connects) {
		if (connectx)
			result = sctp_connectx(sock, serverinfo->ai_addr,
					       1, NULL);
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
	}

	if (hints.ai_socktype == SOCK_STREAM) {

		result = write(sock, &byte, 1);
		if (result < 0) {
			perror("Client write");
			close(sock);
			exit(10);
		}
		if (verbose)
			print_context(sock, "Client STREAM write");

		result = read(sock, label, sizeof(label));
		if (result < 0) {
			perror("Client read");
			close(sock);
			exit(11);
		}
		if (verbose) {
			print_context(sock, "Client STREAM read");
			print_ip_option(sock, ipv4, "Client STREAM read");
		}
		if (expect_ipopt)
			expected = get_ip_option(sock, ipv4, &opt_len);

	} else { /* hints.ai_socktype == SOCK_SEQPACKET */

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

		result = sctp_recvmsg(sock, label, sizeof(label),
				      NULL, 0, NULL, NULL);
		if (result < 0) {
			perror("Client sctp_recvmsg");
			close(sock);
			exit(13);
		}
		if (expect_ipopt)
			expected = get_ip_option(sock, ipv4, &opt_len);
	}

	label[result] = 0;
	close(sock);

	if (!expected && !expect_ipopt) {
		result = getcon(&expected);
		if (result < 0) {
			perror("Client getcon");
			exit(14);
		}
	}

	if (strcmp(expected, label)) {
		fprintf(stderr, "Client expected %s, got %s\n",
			expected, label);
		exit(15);
	} else if (verbose) {
		printf("Client received %s\n", label);
	}

	exit(0);
}

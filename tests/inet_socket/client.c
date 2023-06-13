#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <selinux/selinux.h>

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-e expected_msg] protocol addr port\n"
		"\nWhere:\n\t"
		"-e        Optional expected message from server e.g. \"nopeer\".\n\t"
		"          If not present the client context will be used as a\n\t"
		"          comparison with the servers reply.\n\t"
		"protocol  Protocol to use (tcp, udp, or mptcp)\n\t"
		"addr      IPv4 or IPv6 address (e.g. 127.0.0.1 or ::1)\n\t"
		"port      Port for accessing server.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	char byte, label[256], *expected = NULL;
	int sock, result, sockprotocol, opt;
	struct addrinfo hints, *serverinfo;
	struct timeval tm;

	while ((opt = getopt(argc, argv, "e:")) != -1) {
		switch (opt) {
		case 'e':
			expected = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 3)
		usage(argv[0]);

	memset(&hints, 0, sizeof(struct addrinfo));

	if (!strcmp(argv[optind], "tcp")) {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		sockprotocol      = IPPROTO_TCP;
	} else if (!strcmp(argv[optind], "mptcp")) {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		sockprotocol      = IPPROTO_MPTCP;
	} else if (!strcmp(argv[optind], "udp")) {
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		sockprotocol      = IPPROTO_UDP;
	} else {
		usage(argv[0]);
	}

	result = getaddrinfo(argv[optind + 1], argv[optind + 2], &hints,
			     &serverinfo);
	if (result < 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
		exit(2);
	}

	sock = socket(serverinfo->ai_family, serverinfo->ai_socktype,
		      sockprotocol);
	if (sock < 0) {
		perror("socket");
		exit(3);
	}

	tm.tv_sec = 5;
	tm.tv_usec = 0;
	result = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tm, sizeof(tm));
	if (result < 0) {
		perror("setsockopt: SO_SNDTIMEO");
		exit(4);
	}

	result = connect(sock, serverinfo->ai_addr, serverinfo->ai_addrlen);
	if (result < 0) {
		perror("connect");
		close(sock);
		exit(5);
	}

	byte = 0;
	result = write(sock, &byte, 1);
	if (result < 0) {
		perror("write");
		close(sock);
		exit(6);
	}

	if (hints.ai_socktype == SOCK_DGRAM) {
		struct pollfd fds;

		fds.fd = sock;
		fds.events = POLLIN;
		result = poll(&fds, 1, 1000);
		if (result < 0) {
			perror("poll");
			close(sock);
			exit(7);
		} else if (result == 0) {
			fprintf(stderr, "%s: no reply from server\n", argv[0]);
			exit(8);
		}
	}

	result = read(sock, label, sizeof(label));
	if (result < 0) {
		perror("read");
		close(sock);
		exit(9);
	}
	label[result] = 0;

	if (!expected) {
		result = getcon(&expected);
		if (result < 0) {
			perror("getcon");
			close(sock);
			exit(10);
		}
	}

	if (strcmp(expected, label)) {
		fprintf(stderr, "%s:  expected %s, got %s\n",
			argv[0], expected, label);
		exit(11);
	}

	close(sock);
	exit(0);
}

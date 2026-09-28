#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif

__attribute__((noreturn))
void usage(char *progname)
{
	fprintf(stderr, "usage:  %s [-f] protocol port\n", progname);
	exit(1);
}

static const int on = 1;

int
main(int argc, char **argv)
{
	int csock, ssock;
	int result;
	struct sockaddr_in sin;
	socklen_t sinlen;
	int type, protocol, opt;
	unsigned short port;
	bool tcpfastopen = false;

	while ((opt = getopt(argc, argv, "f")) != -1) {
		switch (opt) {
		case 'f':
			tcpfastopen = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind + 1) != 3)
		usage(argv[0]);

	if (!strcmp(argv[optind], "tcp")) {
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
	} else if (!strcmp(argv[optind], "mptcp")) {
		type = SOCK_STREAM;
		protocol = IPPROTO_MPTCP;
	} else if (!strcmp(argv[optind], "udp")) {
		type = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
	} else {
		usage(argv[0]);
	}

	if (protocol == IPPROTO_UDP && tcpfastopen) {
		fprintf(stderr, "TCP Fast Open only works with TCP or MPTCP\n");
		exit(1);
	}

	port = atoi(argv[optind + 1]);
	if (!port)
		usage(argv[0]);

	ssock = socket(AF_INET, type, protocol);
	if (ssock < 0) {
		perror("socket");
		exit(1);
	}

	bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	if (inet_aton("127.0.0.1", &sin.sin_addr) == 0) {
		fprintf(stderr, "%s: inet_ntoa: invalid address\n", argv[0]);
		close(ssock);
		exit(1);
	}

	result = setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (result < 0) {
		perror("setsockopt: SO_PASSSEC");
		close(ssock);
		exit(1);
	}

	sinlen = sizeof(sin);
	if (bind(ssock, (struct sockaddr *) &sin, sinlen) < 0) {
		perror("bind");
		close(ssock);
		exit(1);
	}

	if (listen(ssock, SOMAXCONN)) {
		perror("listen");
		close(ssock);
		exit(1);
	}

	csock = socket(AF_INET, SOCK_STREAM, 0);
	if (csock < 0) {
		perror("socket");
		close(ssock);
		exit(1);
	}

	if (tcpfastopen) {
		char byte = 0;
		result = sendto(csock, &byte, 1, MSG_FASTOPEN,
				(struct sockaddr *) &sin, sinlen);
		if (result < 0) {
			perror("sendto");
			close(ssock);
			close(csock);
			exit(1);
		}
	} else {
		result = connect(csock, (struct sockaddr *) &sin, sinlen);
		if (result < 0) {
			perror("connect");
			close(ssock);
			close(csock);
			exit(1);
		}
	}

	close(ssock);
	close(csock);
	exit(0);
}

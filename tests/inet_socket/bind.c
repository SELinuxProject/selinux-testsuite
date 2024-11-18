#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif

__attribute__((noreturn))
void usage(char *progname)
{
	fprintf(stderr, "usage:  %s protocol port\n", progname);
	exit(1);
}

static const int on = 1;

int
main(int argc, char **argv)
{
	int sock;
	int result;
	struct sockaddr_in sin;
	socklen_t sinlen;
	int type, protocol;
	unsigned short port;

	if (argc != 3)
		usage(argv[0]);

	if (!strcmp(argv[1], "tcp")) {
		type = SOCK_STREAM;
		protocol = IPPROTO_TCP;
	} else if (!strcmp(argv[1], "mptcp")) {
		type = SOCK_STREAM;
		protocol = IPPROTO_MPTCP;
	} else if (!strcmp(argv[1], "udp")) {
		type = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
	} else {
		usage(argv[0]);
	}

	port = atoi(argv[2]);
	if (!port)
		usage(argv[0]);

	sock = socket(AF_INET, type, protocol);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (result < 0) {
		perror("setsockopt: SO_PASSSEC");
		close(sock);
		exit(1);
	}

	bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);
	sinlen = sizeof(sin);
	if (bind(sock, (struct sockaddr *) &sin, sinlen) < 0) {
		perror("bind");
		close(sock);
		exit(1);
	}

	close(sock);
	exit(0);
}

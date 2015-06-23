#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s port\n", progname);
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
	unsigned short port;

	if (argc != 2)
		usage(argv[0]);

	port = atoi(argv[1]);
	if (!port)
		usage(argv[0]);

	ssock = socket(AF_INET, SOCK_STREAM, 0);
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

	result = connect(csock, (struct sockaddr *) &sin, sinlen);
	if (result < 0) {
		perror("connect");
		close(ssock);
		close(csock);
		exit(1);
	}
	close(ssock);
	close(csock);
	exit(0);
}

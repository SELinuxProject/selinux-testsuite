#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
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
#include <selinux/selinux.h>

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [stream|dgram] port\n",
		progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	char byte, label[256];
	int sock;
	int result;
	struct sockaddr_in sin;
	socklen_t sinlen;
	int type;
	char *mycon;
	unsigned short port;

	if (argc != 3)
		usage(argv[0]);

	if (!strcmp(argv[1], "stream"))
		type = SOCK_STREAM;
	else if (!strcmp(argv[1], "dgram"))
		type = SOCK_DGRAM;
	else
		usage(argv[0]);

	port = atoi(argv[2]);
	if (!port)
		usage(argv[0]);

	sock = socket(AF_INET, type, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	bzero(&sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	if (inet_aton("127.0.0.1", &sin.sin_addr) == 0) {
		fprintf(stderr, "%s: inet_ntoa: invalid address\n", argv[0]);
		close(sock);
		exit(1);
	}

	sinlen = sizeof(sin);
	result = connect(sock, (struct sockaddr *) &sin, sinlen);
	if (result < 0) {
		perror("connect");
		close(sock);
		exit(1);
	}

	byte = 0;
	result = write(sock, &byte, 1);
	if (result < 0) {
		perror("write");
		close(sock);
		exit(1);
	}

	if (type == SOCK_DGRAM) {
		struct pollfd fds;

		fds.fd = sock;
		fds.events = POLLIN;
		result = poll(&fds, 1, 1000);
		if (result < 0) {
			perror("poll");
			close(sock);
			exit(1);
		} else if (result == 0) {
			fprintf(stderr, "%s: no reply from server\n", argv[0]);
			exit(1);
		}
	}

	result = read(sock, label, sizeof(label));
	if (result < 0) {
		perror("read");
		close(sock);
		exit(1);
	}
	label[result] = 0;

	result = getcon(&mycon);
	if (result < 0) {
		perror("getcon");
		close(sock);
		exit(1);
	}

	if (strcmp(mycon, label)) {
		fprintf(stderr, "%s:  expected %s, got %s\n",
			argv[0], mycon, label);
		exit(1);
	}

	close(sock);
	exit(0);
}

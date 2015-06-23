#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

void usage(char *progname)
{
	fprintf(stderr, "usage:  %s [stream|dgram] port\n", progname);
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
	int type;
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

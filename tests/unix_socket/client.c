#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <selinux/selinux.h>

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-a] [stream|dgram] local-socket-name remote-socket-name\n",
		progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	char byte, label[256];
	int sock;
	int result;
	struct sockaddr_un sun, remotesun;
	socklen_t sunlen, remotesunlen;
	int type;
	char *mycon;
	int opt;
	bool abstract = false;

	while ((opt = getopt(argc, argv, "a")) != -1) {
		switch (opt) {
		case 'a':
			abstract = true;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	if ((argc - optind) != 3)
		usage(argv[0]);

	if (!strcmp(argv[optind], "stream"))
		type = SOCK_STREAM;
	else if (!strcmp(argv[optind], "dgram"))
		type = SOCK_DGRAM;
	else
		usage(argv[0]);

	sock = socket(AF_UNIX, type, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	bzero(&sun, sizeof(struct sockaddr_un));
	sun.sun_family = AF_UNIX;
	if (abstract) {
		sun.sun_path[0] = 0;
		strcpy(&sun.sun_path[1], argv[optind + 1]);
		sunlen = offsetof(struct sockaddr_un, sun_path) +
			 strlen(&sun.sun_path[1]) + 1;
	} else {
		strcpy(sun.sun_path, argv[optind + 1]);
		unlink(sun.sun_path);
		sunlen = offsetof(struct sockaddr_un, sun_path) +
			 strlen(sun.sun_path) + 1;
	}

	if (bind(sock, (struct sockaddr *) &sun, sunlen) < 0) {
		perror("bind");
		close(sock);
		exit(1);
	}

	bzero(&remotesun, sizeof(struct sockaddr_un));
	remotesun.sun_family = AF_UNIX;
	if (abstract) {
		remotesun.sun_path[0] = 0;
		strcpy(&remotesun.sun_path[1], argv[optind + 2]);
		remotesunlen = offsetof(struct sockaddr_un,
					sun_path) + strlen(&remotesun.sun_path[1]) + 1;
	} else {
		strcpy(remotesun.sun_path, argv[optind + 2]);
		remotesunlen = offsetof(struct sockaddr_un, sun_path) +
			       strlen(remotesun.sun_path) + 1;
	}

	result = connect(sock, (struct sockaddr *) &remotesun, remotesunlen);
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
	if (!abstract)
		unlink(sun.sun_path);
	exit(0);
}

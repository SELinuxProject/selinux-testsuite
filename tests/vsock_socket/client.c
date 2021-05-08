#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

// Must be included after sys/socket.h
#include <linux/vm_sockets.h>

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s cid port\n"
		"\nWhere:\n\t"
		"cid     Context Identifier of the server\n\t"
		"port    Server port\n", progname);
	exit(1);
}

#define TEST_VALUE	42

int
main(int argc, char **argv)
{
	unsigned cid;
	unsigned port;
	int sock;
	int opt;
	int result;
	char byte;
	socklen_t len;
	uint64_t bufsize;
	struct sockaddr_vm svm;

	while ((opt = getopt(argc, argv, ":")) != -1) {
		switch (opt) {
		default:
			usage(argv[0]);
			break;
		}
	}

	if ((argc - optind) != 2)
		usage(argv[0]);

	cid = strtoul(argv[optind], NULL, 10);
	if (!cid)
		usage(argv[0]);

	port = strtoull(argv[optind + 1], NULL, 10);
	if (!port)
		usage(argv[0]);

	sock = socket(AF_VSOCK, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(2);
	}

	bzero(&svm, sizeof(svm));
	svm.svm_family = AF_VSOCK;
	svm.svm_port = port;
	svm.svm_cid = cid;

	result = connect(sock, (struct sockaddr *)&svm, sizeof(svm));
	if (result < 0) {
		perror("connect");
		close(sock);
		exit(3);
	}

	byte = TEST_VALUE;
	result = write(sock, &byte, 1);
	if (result < 0) {
		perror("write");
		close(sock);
		exit(4);
	}

	result = shutdown(sock, SHUT_WR);
	if (result < 0) {
		perror("shutdown");
		close(sock);
		exit(5);
	}

	result = read(sock, &byte, 1);
	if (result < 0) {
		perror("read");
		close(sock);
		exit(6);
	} else if (result != 1) {
		fprintf(stderr, "%s: expected 1 byte, got %d\n",
			argv[0], result);
		close(sock);
		exit(1);
	} else if (byte != TEST_VALUE) {
		fprintf(stderr, "%s: expected %d, got %d\n",
			argv[0], TEST_VALUE, byte);
		exit(1);
	}

	len = sizeof(svm);
	result = getsockname(sock, (struct sockaddr *)&svm, &len);
	if (result < 0) {
		perror("getsockname");
		close(sock);
		exit(7);
	}

	result = getsockopt(sock, AF_VSOCK, SO_VM_SOCKETS_BUFFER_SIZE, &bufsize, &len);
	if (result < 0) {
		perror("getsockopt");
		close(sock);
		exit(8);
	}

	result = setsockopt(sock, AF_VSOCK, SO_VM_SOCKETS_BUFFER_SIZE, &bufsize, len);
	if (result < 0) {
		perror("setsockopt");
		close(sock);
		exit(9);
	}

	close(sock);
	exit(0);
}

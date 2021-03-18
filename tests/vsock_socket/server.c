#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

// Must be included after sys/socket.h
#include <linux/vm_sockets.h>

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-f file] [-p port]\n"
		"\nWhere:\n\t"
		"-f      Flag file signaling server readiness\n\t"
		"-p      Listening port, otherwise random\n\t"
		"-s      Single-client mode\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	unsigned port = VMADDR_PORT_ANY;
	int sock;
	int opt;
	int result;
	struct sockaddr_vm svm;
	socklen_t svmlen;
	char *flag_file = NULL;
	int single_client = 0;

	while ((opt = getopt(argc, argv, "p:f:s")) != -1) {
		switch (opt) {
		case 'p':
			port = strtoul(optarg, NULL, 10);
			if (!port)
				usage(argv[0]);
			break;
		case 'f':
			flag_file = optarg;
			break;
		case 's':
			single_client = 1;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	if ((argc - optind) != 0)
		usage(argv[0]);

	sock = socket(AF_VSOCK, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(2);
	}

	bzero(&svm, sizeof(svm));
	svm.svm_family = AF_VSOCK;
	svm.svm_port = port;
	svm.svm_cid = VMADDR_CID_LOCAL;

	svmlen = sizeof(svm);
	if (bind(sock, (struct sockaddr *)&svm, svmlen) < 0) {
		perror("bind");
		close(sock);
		exit(3);
	}

	/* Update svm with the port number selected by the kernel. */
	if (getsockname(sock, (struct sockaddr *)&svm, &svmlen) < 0) {
		perror("getsockname");
		close(sock);
		exit(4);
	}

	if (listen(sock, SOMAXCONN)) {
		perror("listen");
		close(sock);
		exit(5);
	}

	if (flag_file) {
		FILE *f = fopen(flag_file, "w");
		if (!f) {
			perror("Flag file open");
			close(sock);
			exit(1);
		}
		fprintf(f, "%u\n", svm.svm_port);
		fclose(f);
	}

	for(;;) {
		char byte;
		int newsock;
		int pid;
		struct sockaddr_vm newsvm;
		socklen_t newsvmlen;

		newsvmlen = sizeof(newsvm);
		newsock = accept(sock, (struct sockaddr *)&newsvm, &newsvmlen);
		if (newsock < 0) {
			perror("accept");
			close(sock);
			exit(6);
		}

		if (!single_client) {
			pid = fork();
			if (pid < 0) {
				perror("fork");
				close(sock);
				exit(7);
			} else if (pid > 0) {
				close(newsock);
				continue;
			}
		}

		result = read(newsock, &byte, 1);
		if (result < 0) {
			perror("read");
			exit(8);
		}

		result = write(newsock, &byte, 1);
		if (result < 0 && errno != EPIPE) {
			perror("write");
			exit(9);
		}

		close(newsock);
		exit(0);
	}
}

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Must be included after sys/socket.h
#include <linux/vm_sockets.h>

int main(int argc, char **argv)
{
	int sock;
	struct sockaddr_vm svm;

	sock = socket(AF_VSOCK, SOCK_STREAM, 0);
	if (sock < 0) {
		if (errno == EAFNOSUPPORT) {
			// AF_VSOCK not supported
			exit(2);
		} else {
			perror("socket");
			exit(1);
		}
	}

	bzero(&svm, sizeof(svm));
	svm.svm_family = AF_VSOCK;
	svm.svm_port = VMADDR_PORT_ANY;
	svm.svm_cid = VMADDR_CID_LOCAL;

	if (bind(sock, (struct sockaddr *)&svm, sizeof(svm)) < 0) {
		if (errno == EADDRNOTAVAIL) {
			// vsock_loopback not supported
			close(sock);
			exit(3);
		} else {
			perror("bind");
			close(sock);
			exit(1);
		}
	}

	close(sock);
	exit(0);
}

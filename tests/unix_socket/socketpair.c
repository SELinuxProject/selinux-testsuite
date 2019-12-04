#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <selinux/selinux.h>

#ifndef SO_PEERSEC
#define SO_PEERSEC 31
#endif

#ifndef SCM_SECURITY
#define SCM_SECURITY 0x03
#endif

void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s stream|dgram\n"
		"Where:\n\t"
		"stream     Use TCP protocol or:\n\t"
		"dgram      use UDP protocol.\n", progname);
	exit(-1);
}

int run_parent(int sock, int type)
{
	int result, on = 1;
	char byte, peerlabel[256];
	socklen_t labellen = sizeof(peerlabel);

	result = setsockopt(sock, SOL_SOCKET, SO_PASSSEC, &on, sizeof(on));
	if (result < 0) {
		perror("setsockopt: SO_PASSSEC");
		goto err;
	}

	if (type == SOCK_STREAM) {
		result = read(sock, &byte, 1);
		if (result < 0) {
			perror("read");
			goto err;
		}

		peerlabel[0] = 0;
		result = getsockopt(sock, SOL_SOCKET, SO_PEERSEC, peerlabel,
				    &labellen);
		if (result < 0) {
			perror("getsockopt: SO_PEERSEC");
			goto err;
		}
		printf("Parent got peer label=%s\n", peerlabel);

		result = write(sock, peerlabel, strlen(peerlabel));
		if (result < 0) {
			perror("write");
			goto err;
		}
	} else {
		struct iovec iov;
		struct msghdr msg;
		struct cmsghdr *cmsg;
		char msglabel[256];
		union {
			struct cmsghdr cmsghdr;
			char buf[CMSG_SPACE(sizeof(msglabel))];
		} control;

		memset(&iov, 0, sizeof(iov));
		iov.iov_base = &byte;
		iov.iov_len = 1;
		memset(&msg, 0, sizeof(msg));
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = &control;
		msg.msg_controllen = sizeof(control);
		result = recvmsg(sock, &msg, 0);
		if (result < 0) {
			perror("recvmsg");
			goto err;
		}

		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
		     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			;
			if (cmsg->cmsg_level == SOL_SOCKET &&
			    cmsg->cmsg_type == SCM_SECURITY) {
				size_t len = cmsg->cmsg_len - CMSG_LEN(0);

				if (len > 0 && len < sizeof(msglabel)) {
					memcpy(msglabel, CMSG_DATA(cmsg), len);
					msglabel[len] = 0;
					printf("Parent got SCM_SECURITY=%s\n",
					       msglabel);
				}
			}
		}

		result = sendto(sock, msglabel, strlen(msglabel), 0,
				msg.msg_name, msg.msg_namelen);
		if (result < 0) {
			perror("sendto");
			goto err;
		}
	}

	result = 0;
err:
	close(sock);
	return result;
}

int run_child(int sock)
{
	int result;
	char byte = 0, label[256], *mycon;

	result = write(sock, &byte, 1);
	if (result < 0) {
		perror("write");
		goto err;
	}

	result = read(sock, label, sizeof(label));
	if (result < 0) {
		perror("read");
		goto err;
	}
	label[result] = 0;

	result = getcon(&mycon);
	if (result < 0) {
		perror("getcon");
		goto err1;
	}

	if (strcmp(mycon, label)) {
		fprintf(stderr, "Child expected %s, got %s\n", mycon, label);
		result = -1;
		goto err1;
	}

	result = 0;
err1:
	free(mycon);
err:
	close(sock);
	return result;
}

int main(int argc, char **argv)
{
	int type, result, sockets[2];

	if (argc != 2)
		print_usage(argv[0]);

	if (!strcmp(argv[1], "stream"))
		type = SOCK_STREAM;
	else if (!strcmp(argv[1], "dgram"))
		type = SOCK_DGRAM;
	else
		print_usage(argv[0]);

	result = socketpair(AF_UNIX, type, 0, sockets);
	if (result < 0) {
		perror("socketpair");
		return -1;
	}

	result = fork();
	if (result < 0) {
		perror("fork");
		close(sockets[0]);
		close(sockets[1]);
		return -1;
	} else if (result > 0) {
		close(sockets[0]);
		result = run_parent(sockets[1], type);
		return result;

	} else {
		close(sockets[1]);
		result = run_child(sockets[0]);
		return result;
	}
}

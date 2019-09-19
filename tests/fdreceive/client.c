#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#if HAVE_BPF
#include "../bpf/bpf_common.h"
#endif

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-m|-p|t] [file] addr\n"
		"\nWhere:\n\t"
		"-m    Create BPF map fd\n\t"
		"-p    Create BPF prog fd\n\t"
		"-t    Test if BPF enabled\n\t"
		"   If -m or -p not supplied, create a file fd using:\n\t"
		"file  Test file fd sent to server\n\t"
		"addr  Servers address\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	struct sockaddr_un sun;
	char buf[1024], *addr = NULL;
	int opt, s, sunlen, ret, buflen;
	struct msghdr msg = { 0 };
	struct iovec iov;
	struct cmsghdr *cmsg;
	int myfd = 0;
	char cmsgbuf[CMSG_SPACE(sizeof myfd)];
	int *fdptr;

	enum {
		FILE_FD,
		MAP_FD,
		PROG_FD,
		BPF_TEST
	} client_fd_type;

	client_fd_type = FILE_FD;

	while ((opt = getopt(argc, argv, "mpt")) != -1) {
		switch (opt) {
		case 'm':
			client_fd_type = MAP_FD;
			break;
		case 'p':
			client_fd_type = PROG_FD;
			break;
		case 't':
			client_fd_type = BPF_TEST;
			break;
		}
	}

	if ((client_fd_type == FILE_FD && (argc - optind) != 2) ||
	    (client_fd_type > FILE_FD && (argc - optind) != 1))
		usage(argv[0]);

	switch (client_fd_type) {
	case FILE_FD:
		myfd = open(argv[optind], O_RDWR);
		if (myfd < 0) {
			perror(argv[optind]);
			exit(-1);
		}

		addr = argv[optind + 1];
		printf("client: Using a file fd\n");
		break;
#if HAVE_BPF
	case MAP_FD:
		/* If BPF enabled, then need to set limits */
		bpf_setrlimit();
		addr = argv[optind];
		myfd = create_bpf_map();
		printf("client: Using a BPF map fd\n");
		break;
	case PROG_FD:
		bpf_setrlimit();
		addr = argv[optind];
		myfd = create_bpf_prog();
		printf("client: Using a BPF prog fd\n");
		break;
	case BPF_TEST:
		exit(0);
		break;
#else
	case MAP_FD:
	case PROG_FD:
	case BPF_TEST:
		fprintf(stderr, "BPF not supported by Client\n");
		exit(-1);
		break;
#endif
	default:
		usage(argv[0]);
	}

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		perror("socket");
		exit(-1);
	}

	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, addr);

	sunlen = strlen(sun.sun_path) + 1 + sizeof(short);
	ret = connect(s, (struct sockaddr *)&sun, sunlen);
	if (ret < 0) {
		perror("connect");
		exit(-1);
	}

	printf("client: Connected to server via %s\n", sun.sun_path);

	strcpy(buf, "hello world");
	buflen = strlen(buf) + 1;
	iov.iov_base = buf;
	iov.iov_len = buflen;
	msg.msg_name = 0;
	msg.msg_namelen = 0;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof cmsgbuf;
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	fdptr = (int *)CMSG_DATA(cmsg);
	memcpy(fdptr, &myfd, sizeof(int));
	msg.msg_controllen = cmsg->cmsg_len;

	ret = sendmsg(s, &msg, 0);
	if (ret < 0) {
		perror("sendmsg");
		exit(-1);
	}
	printf("client: Sent descriptor, waiting for reply\n");

	buf[0] = 0;
	ret = recv(s, buf, sizeof(buf), 0);
	if (ret < 0) {
		perror("recv");
		exit(-1);
	}
	printf("client: Received reply, code=%d\n", buf[0]);
	if (buf[0])
		printf("client: ...This implies the descriptor was not received\n");
	else
		printf("client: ...This implies the descriptor was received\n");

	exit(buf[0]);
}

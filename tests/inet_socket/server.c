#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef SO_PEERSEC
#define SO_PEERSEC 31
#endif

#ifndef SCM_SECURITY
#define SCM_SECURITY 0x03
#endif

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-f file] [-n] protocol port\n"
		"\nWhere:\n\t"
		"-f        Write a line to the file when listening starts.\n\t"
		"-n        No peer context will be available therefore send\n\t"
		"          \"nopeer\" message to client, otherwise the peer context\n\t"
		"          will be retrieved and sent to client.\n\t"
		"protocol  Protocol to use (tcp, udp, or mptcp)\n\t"
		"port      Listening port\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int sock, result, opt, sockprotocol, on = 1;
	socklen_t sinlen;
	struct sockaddr_storage sin;
	struct addrinfo hints, *res;
	char byte;
	bool nopeer = false;
	char *flag_file = NULL;

	while ((opt = getopt(argc, argv, "f:n")) != -1) {
		switch (opt) {
		case 'f':
			flag_file = optarg;
			break;
		case 'n':
			nopeer = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 2)
		usage(argv[0]);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET6;

	if (!strcmp(argv[optind], "tcp")) {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		sockprotocol      = IPPROTO_TCP;
	} else if (!strcmp(argv[optind], "mptcp")) {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		sockprotocol      = IPPROTO_MPTCP;
	} else if (!strcmp(argv[optind], "udp")) {
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		sockprotocol      = IPPROTO_UDP;
	} else {
		usage(argv[0]);
	}

	result = getaddrinfo(NULL, argv[optind + 1], &hints, &res);
	if (result < 0) {
		printf("getaddrinfo: %s\n", gai_strerror(result));
		exit(1);
	}

	sock = socket(res->ai_family, res->ai_socktype, sockprotocol);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	/* Allow retrieval of UDP/Datagram security contexts for IPv4 as
	 * IPv6 is not currently supported.
	 */
	if (hints.ai_socktype == SOCK_DGRAM) {
		result = setsockopt(sock, SOL_IP, IP_PASSSEC, &on, sizeof(on));
		if (result < 0) {
			perror("setsockopt: IP_PASSSEC");
			close(sock);
			exit(1);
		}
	}

	result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (result < 0) {
		perror("setsockopt: SO_REUSEADDR");
		close(sock);
		exit(1);
	}

	if (bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		close(sock);
		exit(1);
	}

	if (hints.ai_socktype == SOCK_STREAM) {
		if (listen(sock, SOMAXCONN)) {
			perror("listen");
			close(sock);
			exit(1);
		}
	}

	if (flag_file) {
		FILE *f = fopen(flag_file, "w");
		if (!f) {
			perror("Flag file open");
			close(sock);
			exit(1);
		}
		fprintf(f, "listening\n");
		fclose(f);
	}

	if (hints.ai_socktype == SOCK_STREAM) {
		do {
			int newsock;
			char peerlabel[256];
			socklen_t labellen = sizeof(peerlabel);

			sinlen = sizeof(sin);
			newsock = accept(sock, (struct sockaddr *)&sin, &sinlen);
			if (newsock < 0) {
				perror("accept");
				close(sock);
				exit(1);
			}

			if (nopeer) {
				strcpy(peerlabel, "nopeer");
			} else {
				peerlabel[0] = 0;
				result = getsockopt(newsock, SOL_SOCKET,
						    SO_PEERSEC, peerlabel,
						    &labellen);
				if (result < 0) {
					perror("getsockopt: SO_PEERSEC");
					exit(1);
				}

				printf("%s:  Got peer label=%s\n", argv[0], peerlabel);
			}

			result = read(newsock, &byte, 1);
			if (result < 0) {
				perror("read");
				exit(1);
			}

			result = write(newsock, peerlabel, strlen(peerlabel));
			if (result < 0) {
				perror("write");
				exit(1);
			}
			close(newsock);
		} while (1);
	} else {
		struct iovec iov;
		struct msghdr msg;
		struct cmsghdr *cmsg;
		char msglabel[256];
		union {
			struct cmsghdr cmsghdr;
			char buf[CMSG_SPACE(sizeof(msglabel))];
		} control;

		do {
			memset(&iov, 0, sizeof(iov));
			iov.iov_base = &byte;
			iov.iov_len = 1;
			memset(&msg, 0, sizeof(msg));
			msglabel[0] = 0;
			msg.msg_name = &sin;
			msg.msg_namelen = sizeof(sin);
			msg.msg_iov = &iov;
			msg.msg_iovlen = 1;
			msg.msg_control = &control;
			msg.msg_controllen = sizeof(control);
			result = recvmsg(sock, &msg, 0);
			if (result < 0) {
				perror("recvmsg");
				exit(1);
			}
			if (nopeer) {
				strcpy(msglabel, "nopeer");
			}
			for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
			     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
				if (cmsg->cmsg_level == SOL_IP &&
				    cmsg->cmsg_type == SCM_SECURITY) {
					size_t len = cmsg->cmsg_len - CMSG_LEN(0);

					if (len > 0 && len < sizeof(msglabel)) {
						memcpy(msglabel, CMSG_DATA(cmsg), len);
						msglabel[len] = 0;
						printf("%s: Got SCM_SECURITY=%s\n",
						       argv[0], msglabel);
					}
				}
			}
			result = sendto(sock, msglabel, strlen(msglabel), 0,
					msg.msg_name, msg.msg_namelen);
			if (result < 0) {
				perror("sendto");
				exit(1);
			}
		} while (1);
	}

	close(sock);
	exit(0);
}

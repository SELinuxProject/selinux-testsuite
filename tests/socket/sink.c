#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* for inet_aton() */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include<fs_secure.h>		/* for SELinux extended syscalls */

static const char *hostname = "127.0.0.1";
static const u_short port = 4000;
static const int readbytes = 100;
static const int on = 1;

int
main(int argc, char **argv)
{
	char buff[65536];
	struct sockaddr_in saddr, raddr;
	socklen_t rsize = sizeof (raddr);
	int currentbytes = 0;
	int sock;
	int protocol;
	int result;
	security_id_t sid = -1;

	if (argc < 2) {
		printf("%s <protocol>\n", argv[0]);
		return -1;
	}

	if (strcmp(argv[1], "stream") == 0)
		protocol = SOCK_STREAM;
	else
		protocol = SOCK_DGRAM;

	if (argc == 3)
		sid = atoi(argv[2]);

	bzero(&saddr, sizeof (struct sockaddr_in));
	if (!inet_aton(hostname, &saddr.sin_addr)) {
		printf("error converting address\n");
		return -1;
	}

	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);

	if ((sock = socket(saddr.sin_family, protocol, 0)) < 0) {
		perror("socket");
		return -1;
	}

	if ((result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				 &on, sizeof (on)))) {
		perror("setsockopt: ");
		close(sock);
		return -1;
	}
	if (bind(sock, (struct sockaddr *) &saddr, sizeof (saddr)) < 0) {
		perror("bind");
		close(sock);
		return -1;
	}

	if (sid != -1) {
		struct stat statbuf;
		security_id_t outsid;
		(void)fstat_secure(sock, &statbuf, &outsid);
		printf("Changing socket sid from %d to %d\n", outsid, sid);
		(void)fchsid(sock, sid);
	}

	if (protocol == SOCK_STREAM) {
		int remote;
		if (listen(sock, SOMAXCONN)) {
			close(sock);
			return -1;
		}
		while (readbytes > currentbytes) {
			remote =
				accept(sock, (struct sockaddr *) &raddr, &rsize);
			if (remote < 0) {
				perror("accept: ");
				close(sock);
				return -1;
			}
			for (;;) {
				result = read(remote, buff, 65536);
				if (result == 0)	// EOF
					break;
				if (result < 0) {
					perror("stream read failed: ");
					close(sock);
					return -1;
				}
				currentbytes += result;
				printf("recv: %5d bytes, %5d total\n", result,
				       currentbytes);
				if (currentbytes >= readbytes)
					break;
			}
		}
	} else {
		for (;;) {
			result = recvfrom(sock, buff, 65536, 0,
					  (struct sockaddr *) &raddr, &rsize);
			currentbytes += result;
			printf("recv: %5d bytes, %5d total\n", result,
			       currentbytes);
			if (currentbytes >= readbytes)
				break;
		}
	}

	shutdown(sock, 2);
	close(sock);
	return (0);
}

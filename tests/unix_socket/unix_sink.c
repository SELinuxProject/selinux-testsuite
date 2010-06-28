#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* for inet_aton() */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include<fs_secure.h>		/* for SELinux extended syscalls */

static const char *filename = "unix_test_socket";
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
	int sunlen;
	security_id_t sid = -1;
	struct sockaddr_un sun;
	int abstract = 0;

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

	if ((sock = socket(AF_UNIX, protocol, 0)) < 0) {
		perror("socket");
		return -1;
	}

	if ((result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
				 &on, sizeof (on)))) {
		perror("setsockopt: ");
		close(sock);
		return -1;
	}

	bzero(&sun, sizeof (struct sockaddr_un));
	sun.sun_family = AF_UNIX;
	if (abstract) {
		sun.sun_path[0] = 0;
		strcpy(sun.sun_path + 1, filename);
		sunlen = strlen(sun.sun_path + 1) + 1 + sizeof (short);
	} else {
		strcpy(sun.sun_path, filename);
		sunlen = strlen(sun.sun_path) + 1 + sizeof (short);
	}

	if (bind(sock, (struct sockaddr *) &sun, sunlen) < 0) {
		perror("bind");
		close(sock);
		return -1;
	}

	if (sid != -1) {
		struct stat statbuf;
		security_id_t outsid;
		(void) fstat_secure(sock, &statbuf, &outsid);
		printf("Changing socket sid from %d to %d\n", outsid, sid);
		(void) fchsid(sock, sid);
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

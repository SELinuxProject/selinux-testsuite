#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* for inet_aton() */
#include <sys/poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include<fs_secure.h>		/* for SELinux extended syscalls */

static const char *hostname = "127.0.0.1";
static const u_short port = 4000;
static const int on = 1;

int
main(int argc, char **argv)
{
	struct sockaddr_in saddr, raddr;
	socklen_t rsize = sizeof (raddr);
	int sock;
	int result;
	int remote;
	security_id_t sid = -1;
	struct stat statbuf;
	security_id_t outsid;

	if (argc != 2) {
		printf("%s <sid>\n", argv[0]);
		return -1;
	}
	sid = atoi(argv[1]);

	bzero(&saddr, sizeof (struct sockaddr_in));
	if (!inet_aton(hostname, &saddr.sin_addr)) {
		printf("error converting address\n");
		return -1;
	}


	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);

	if ((sock = socket(saddr.sin_family, SOCK_STREAM, 0)) < 0) {
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

	if (listen(sock, SOMAXCONN)) {
		close(sock);
		return -1;
	}

	if ((result = fcntl(sock, F_SETFL, O_NONBLOCK))) {
		perror("fcntl: ");
		return -1;
	}

	(void)fstat_secure(sock, &statbuf, &outsid);
	printf("Changing socket sid from %d to %d\n", outsid, sid);
	(void)fchsid(sock, sid);

	remote = accept(sock, (struct sockaddr *) &raddr, &rsize);

	printf("Changing socket sid back to %d\n", outsid);
	(void)fchsid(sock, outsid);

	printf("remote == %d\n", remote);

	shutdown(sock, 2);
	close(sock);

	if (remote == -1)
		return 1;

	return (0);
}

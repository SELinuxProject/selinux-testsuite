#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* for inet_aton() */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include <socket_secure.h>
#include <ss.h>

static const char *filename = "unix_test_socket";
static const int readbytes = 2000;
static const int on = 1;

#define MAXSTAT 10
static security_id_t sids[MAXSTAT];
static unsigned int conncount[MAXSTAT];

void
initStats()
{
	memset(sids, '\0', MAXSTAT * sizeof (security_id_t));
	memset(conncount, '\0', MAXSTAT * sizeof (unsigned int));
}

static inline void
updateStats(security_id_t sid)
{
	int i;
	for (i = 0; i < MAXSTAT; i++) {
		if (sids[i] == sid) {
			conncount[i]++;
			return;
		}
	}
	/* not found */
	for (i = 0; i < MAXSTAT; i++) {
		if (sids[i] == 0) {
			sids[i] = sid;
			conncount[i]++;
			return;
		}
	}
	printf("sid table out of space\n");
}

void
printStats()
{
	int i;
	char context[255];
	int contextlen;

	/* First the script-parseable output */
	for (i = 0; i < MAXSTAT; i++) {
		if (sids[i] != 0) {
			printf("(%d,%d) ", sids[i], conncount[i]);
		}
	}
	printf("\nServer connection statistics:\n");
	printf("  count    SID  Context\n");
	for (i = 0; i < MAXSTAT; i++) {
		if (sids[i] != 0) {
			contextlen = sizeof (context);
			if (security_sid_to_context
			    (sids[i], context, &contextlen)) {
				perror("security_sid_to_context");
				exit(1);
			}
			printf("%7d  %5d  %s\n", conncount[i], sids[i],
			       context);
		}
	}
}

int
main(int argc, char **argv)
{
	char buff[65536], context[255];
	int maxcons = 100;
	int sunlen, rsize, remote, sock, result, contextlen, connections;
	security_id_t socket_sid = 0;
	security_id_t peer_sid;
	struct sockaddr_un sun, raddr;

	initStats();
	if (argc > 1)
		maxcons = atoi(argv[1]);
	if (argc > 2)
		socket_sid = atoi(argv[2]);

	if (socket_sid)
		sock = socket_secure(AF_UNIX, SOCK_STREAM, 0, socket_sid);
	else
		sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock < 0) {
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
	strcpy(sun.sun_path, filename);
	sunlen = strlen(sun.sun_path) + 1 + sizeof (short);

	if (bind(sock, (struct sockaddr *) &sun, sunlen) < 0) {
		perror("bind");
		close(sock);
		return -1;
	}

	if (listen(sock, SOMAXCONN)) {
		close(sock);
		return -1;
	}

	connections = 0;
	while (connections < maxcons) {
		connections++;
		rsize = sizeof (raddr);
		remote = accept_secure(sock, (struct sockaddr *) &raddr,
				       &rsize, &peer_sid);
		if (remote < 0) {
			perror("accept: ");
			close(sock);
			return -1;
		}
		contextlen = sizeof (context);
		result = security_sid_to_context(peer_sid, context,
						 &contextlen);
		if (result) {
			perror("security_sid_to_context");
			exit(1);
		}

		updateStats(peer_sid);
		for (;;) {
			result = read(remote, buff, 65536);
			if (result == 0)	// EOF
				break;
			if (result < 0) {
				perror("stream read failed: ");
				close(sock);
				return -1;
			}
			result = snprintf(buff, 65536, "%d", peer_sid);
			result = write(remote, buff, result + 1);
		}
		close(remote);
		/* usleep (100000); */
	}

	shutdown(sock, 2);
	close(sock);
	printStats();
	return (0);
}

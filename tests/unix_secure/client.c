#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>		/* for inet_aton() */
#include <sys/poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <socket_secure.h>
#include <ss.h>

static const char *filename = "unix_test_socket";
static const int sendbytes = 100;	/* Send this much data */

int
main(int argc, char **argv)
{
	char data[sendbytes];
	struct sockaddr_un sun, raddr;
	int rsize;
	int bad = 0;
	int sunlen, sock, result;
	struct pollfd fdset;
	security_id_t socket_sid = 0;
	security_id_t peer_sid = 0;
	security_id_t client_sid, read_sid;

	memset(data, 'X', sendbytes);

	if (argc > 1)
		socket_sid = atoi(argv[1]);
	if (argc > 2)
		peer_sid = atoi(argv[2]);

	if (socket_sid)
		sock = socket_secure(AF_UNIX, SOCK_STREAM, 0, socket_sid);
	else
		sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock < 0) {
		perror("socket");
		return -1;
	}

	rsize = sizeof (struct sockaddr_un);
	result = getsockname_secure(sock, (struct sockaddr *) &raddr,
				    &rsize, &client_sid);
	if (result) {
		perror("getsockname_secure");
		exit(1);
	}

	bzero(&sun, sizeof (struct sockaddr_un));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, filename);
	sunlen = strlen(sun.sun_path) + 1 + sizeof (short);

	if (peer_sid) {
		result = connect_secure(sock, (struct sockaddr *) &sun,
					sunlen, peer_sid);
		/* printf("connect_secure with peer sid %d\n", peer_sid); */
	} else {
		result =
		    connect(sock, (struct sockaddr *) &sun, sunlen);
	}
	if (result < 0) {
		if (errno == EINPROGRESS) {
			fdset.fd = sock;
			fdset.events = POLLIN | POLLOUT;
			result = poll(&fdset, 1, 3000);
			if (result < 0) {
				perror("poll: ");
				return -1;
			} else if (result == 0) {
				/* 
				 * connect is taking too long, 
				 * it's probably not going to work 
				 */
				printf("poll: no events are ready\n");
				return -1;
			} else if (result == 1 && fdset.revents & POLLHUP) {
				printf("poll: connection closed\n");
				return -1;
			}
		} else {
			perror("connect");
			close(sock);
			return -1;
		}
	}

	result = write(sock, data, sendbytes);
	if (result != sendbytes) {
		printf("Bad write %d of %d\n", result, sendbytes);
		return -1;
	}

	result = read(sock, data, 65536);
	if (result <= 0) {
		perror("stream read failed: ");
		close(sock);
		return -1;
	}

	read_sid = atoi(data);
	if (read_sid != client_sid) {
		bad++;
		/* printf("client sid %d, read back sid %d\n", 
		   client_sid, read_sid); */
	}

	shutdown(sock, 2);
	close(sock);

	/* 
	 * Return the number of bad connections, where the server
	 * returned something other than the sid of this client's
	 * socket 
	 */
	return (bad);
}

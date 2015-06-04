#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include <socket_secure.h>

int
main(int argc, char **argv)
{
	int sock;
	security_id_t socket_sid = 0;

	sock = socket_secure(AF_INET, SOCK_STREAM, 0, socket_sid);

	if (sock < 0) {
		perror("socket");
		if (errno == ENOSYS) {
			printf("Extended socket system calls are unimplemented\n");
			return -1;
		}
		return -2;
	}
	close(sock);
	printf("Extended socket syscalls present.\n");
	return (0);
}

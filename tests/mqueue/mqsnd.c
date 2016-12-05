#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>
#include <string.h>

#define MSG "TEST MESSAGE"

/*
 * Tests the reading from a posix mqueue. The first argument is the
 * name of the mqueue to be read (including starting '/').
 */
int main(int argc, char **argv)
{
	mqd_t fd;
	if( argc != 2 ) {
		printf("usage: %s </mqueue name> \n", argv[0]);
		return 2;
	}

	fd = mq_open(argv[1], O_WRONLY | O_NONBLOCK);
	if (fd == -1) {
		printf("mqsnd: mq_open: errno = %d\n", errno);
		return 2;
	}

	if ( mq_send(fd, MSG, strlen(MSG), 0)) {
		printf("mqsnd: mq_send: errno = %d\n", errno);
		return 2;
	}

	mq_close(fd);
	return 0;
}

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <errno.h>

/*
 * Tests the reading from a posix mqueue. The first argument is the
 * name of the mqueue to be read (including starting '/').
 */
int main(int argc, char **argv)
{
	mqd_t fd;
	int size;
	char buffer[20];
	if( argc != 2 ) {
		printf("usage: %s </mqueue name> \n", argv[0]);
		return 2;
	}

	fd = mq_open(argv[1], O_RDONLY | O_NONBLOCK);
	if (fd == -1) {
		printf("mqrcv: mq_open: errno = %d\n", errno);
		return 2;
	}

	size = mq_receive(fd, buffer, 20, NULL);
	if (size == -1) {
		printf("mqrcv: mq_receive: errno = %d\n", errno);
		return 2;
	}

	mq_close(fd);
	return 0;
}

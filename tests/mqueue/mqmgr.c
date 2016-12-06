#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/file.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<linux/unistd.h>
#include<mqueue.h>
#include<errno.h>

/*
 * Managed the creation and distruction of a posix mqueue.
 * The first argument is the name of the mqueue to be managed
 * (including starting '/'). The second argument is the
 * operation. '1' to create, '0' to remove.
 *
 */
int main(int argc, char **argv)
{

	mqd_t fd;
	struct mq_attr attr;

	if( argc != 3 ) {
		printf("usage: %s </mqueue name> <0|1> \n", argv[0]);
		exit(2);
	}

	/* initialize the queue attributes */
	attr.mq_flags = 0;
	attr.mq_maxmsg = 10;
	attr.mq_msgsize = 20;
	attr.mq_curmsgs = 0;


	/* unlink the queue if it exists */
	if(argv[2][0] == '0') {
		mq_unlink(argv[1]);
	} else if(argv[2][0] == '1') {
		fd = mq_open(argv[1], O_CREAT, 0664, &attr);
		if (fd != -1) {
			mq_close(fd);
		} else {
			printf("mqmgr:create:errno = %d\n", errno);
			exit(2);
		}
	} else {
		perror("mqmgr:invalid option");
		exit(2);
	}

	exit(0);

}

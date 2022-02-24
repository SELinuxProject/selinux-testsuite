#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <unistd.h>
#include <errno.h>

/*
 * Test the ioctl() calls on a file whose name is given as the first
 * argument. This program expects the domain it is running under to have
 * wide access to the given file.
 */
int main(int argc, char **argv)
{

	int fd;
	int rc;
	int val = 0;

	fd = open(argv[1], O_RDONLY, 0);

	if(fd == -1) {
		perror("test_ioctl:open");
		exit(1);
	}

	/* This one should hit the FILE__GETATTR or FILE__IOCTL test */
	rc = ioctl(fd, FIGETBSZ, &val);
	if( rc < 0 ) {
		perror("test_ioctl:FIGETBSZ");
		exit(1);
	}

	/* This one should hit the FILE__IOCTL test */
	rc = ioctl(fd, FIOQSIZE, &val);
	if( rc < 0 ) {
		perror("test_ioctl:FIOQSIZE");
		exit(1);
	}

	/* This one should hit the FD__USE or FILE__IOCTL test */
	rc = ioctl(fd, FIONBIO, &val);
	if( rc < 0 ) {
		perror("test_ioctl:FIONBIO");
		exit(1);
	}

	/* This one should hit the FILE__GETATTR or FILE__READ test */
	rc = ioctl(fd, FS_IOC_GETVERSION, &val);
	if( rc < 0 && errno != ENOTTY) {
		perror("test_ioctl:FS_IOC_GETVERSION");
		exit(1);
	}

	/* This one should hit the FILE__SETATTR or FILE__WRITE test */
	val = 0;
	rc = ioctl(fd, FS_IOC_SETVERSION, &val);
	if( rc < 0 && errno != ENOTTY) {
		perror("test_ioctl:FS_IOC_SETVERSION");
		exit(1);
	}

	close(fd);
	exit(0);

}

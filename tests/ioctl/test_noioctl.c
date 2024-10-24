#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/*
 * Test the ioctl() calls on a file whose name is given as the first
 * argument. This version of the program expects some of the ioctl()
 * calls to fail, so if one does succeed, we exit with a bad return code.
 * This program expects the domain it is running as to have only read
 * acess to the given file.
 */
int main(int argc, char **argv)
{
	struct utsname uts;
	int fd;
	int rc, useaccessmode = 1;
	int val;

	if (uname(&uts) < 0) {
		perror("uname");
		exit(1);
	}

	if (strverscmp(uts.release, "2.6.27") >= 0 &&
	    strverscmp(uts.release, "2.6.39") < 0)
		useaccessmode = 0;

	fd = open(argv[1], O_RDONLY, 0);

	if(fd == -1) {
		perror("test_noioctl:open");
		exit(1);
	}

	/* This one should hit the FILE__IOCTL or FILE__GETATTR test and fail. */
	rc = ioctl(fd, FIGETBSZ, &val);
	if( rc == 0 ) {
		printf("test_noioctl:FIGETBSZ");
		exit(1);
	}

	/* This one should hit the FILE__IOCTL test and fail. */
	rc = ioctl(fd, FIOQSIZE, &val);
	if( rc == 0 ) {
		printf("test_noioctl:FIOQSIZE");
		exit(1);
	}

	/*
	 * This one depends on kernel version:
	 * >= 2.6.27 and < 2.6.39:  Should hit the FILE__IOCTL test and fail.
	 * < 2.6.27 or >= 2.6.39:  Should only check FD__USE and succeed.
	 */
	rc = ioctl(fd, FIONBIO, &val);
	if(!rc == !useaccessmode ) {
		printf("test_noioctl:FIONBIO");
		exit(1);
	}

	/*
	 * This one depends on kernel version:
	 * New:  Should hit the FILE__READ test and succeed.
	 * Old:  Should hit the FILE__GETATTR test and fail.
	 */
	rc = ioctl(fd, FS_IOC_GETVERSION, &val);
	if((useaccessmode && rc == 0) ||
	    (!useaccessmode && rc < 0 && errno != ENOTTY) ) {
		perror("test_noioctl:FS_IOC_GETVERSION");
		exit(1);
	}

	/* This one should hit the FILE__WRITE or FILE_SETATTR test and fail. */
	val = 0;
	rc = ioctl(fd, FS_IOC_SETVERSION, &val);
	if( rc == 0 ) {
		perror("test_noioctl:FS_IOC_SETVERSION");
		exit(1);
	}

	close(fd);
	exit(0);

}

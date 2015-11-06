#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/personality.h>

#ifndef READ_IMPLIES_EXEC
#define READ_IMPLIES_EXEC 0x0400000
#endif

int main(int argc, char **argv)
{
	char *ptr;
	int rc;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		exit(1);
	}

	/* clear READ_IMPLIES_EXEC if present, because it skips
	 * check for FILE__EXECMOD in selinux_file_mprotect() */
	rc = personality(0xffffffff);
	if ((rc != -1) && (rc & READ_IMPLIES_EXEC)) {
		rc &= ~READ_IMPLIES_EXEC;
		personality(rc);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		exit(1);
	}

	ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
		   0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	*((char *)ptr) = 42;

	rc = mprotect(ptr, 4096, PROT_READ | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}


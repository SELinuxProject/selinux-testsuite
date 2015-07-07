#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(int argc, char **argv)
{
	char *ptr;
	int rc;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		exit(1);
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

	rc = mprotect(ptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}


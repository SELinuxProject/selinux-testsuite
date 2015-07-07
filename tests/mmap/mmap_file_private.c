#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(int argc, char **argv)
{
	char *ptr;
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

	ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
		   MAP_PRIVATE, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}
	close(fd);
	exit(0);
}


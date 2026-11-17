#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-s]\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	char *ptr;
	int rc, opt, shared = MAP_SHARED;
	int fd;
	const size_t length = 4096;

	while ((opt = getopt(argc, argv, "s")) != -1) {
		switch (opt) {
		case 's':
			shared = MAP_SHARED_VALIDATE;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	if ((argc - optind) != 1)
		usage(argv[0]);

	fd = open(argv[optind], O_RDWR);
	if (fd < 0) {
		perror(argv[1]);
		exit(1);
	}

	ptr = mmap(NULL, length, PROT_READ | PROT_WRITE, shared, fd,
		   0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	rc = mprotect(ptr, length, PROT_READ | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}

	exit(0);
}


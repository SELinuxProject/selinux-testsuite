#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/personality.h>

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-rs] file\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	char *ptr;
	int fd, opt, shared = MAP_SHARED, prot = PROT_READ | PROT_WRITE | PROT_EXEC;
	const size_t length = 4096;

	while ((opt = getopt(argc, argv, "rs")) != -1) {
		switch (opt) {
		case 'r':
			if (personality(READ_IMPLIES_EXEC) == -1) {
				perror("personality");
				exit(1);
			}
			prot &= ~PROT_EXEC;
			break;
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
		perror(argv[optind]);
		exit(1);
	}

	ptr = mmap(NULL, length, prot, shared, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}

	close(fd);
	exit(0);
}


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/personality.h>

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-r] file\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	char *ptr;
	int fd, opt, prot = PROT_READ | PROT_WRITE | PROT_EXEC;

	while ((opt = getopt(argc, argv, "r")) != -1) {
		switch (opt) {
		case 'r':
			if (personality(READ_IMPLIES_EXEC) == -1) {
				perror("personality");
				exit(1);
			}
			prot &= ~PROT_EXEC;
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

	ptr = mmap(NULL, 4096, prot, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		exit(1);
	}
	close(fd);
	exit(0);
}


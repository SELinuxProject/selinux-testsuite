#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include "utils.h"

#ifndef MAP_HUGETLB
# define MAP_HUGETLB 0x40000
#endif

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-s]\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	char *ptr;
	int opt, shared = MAP_SHARED;
	long length = getdefaulthugesize();

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

	if ((argc - optind) != 0)
		usage(argv[0]);

	ptr = mmap(NULL, length, PROT_READ | PROT_WRITE | PROT_EXEC,
		   shared | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap MAP_SHARED");
		exit(1);
	}
	exit(0);
}

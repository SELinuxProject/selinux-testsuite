#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/personality.h>

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-r]\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	char buf[4096];
	int rc, opt, prot = PROT_READ | PROT_WRITE | PROT_EXEC;
	void *ptr;
	long pagesize = sysconf(_SC_PAGESIZE);

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

	ptr = (void *) (((unsigned long) buf) & ~(pagesize - 1));

	rc = mprotect(ptr, pagesize, prot);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}


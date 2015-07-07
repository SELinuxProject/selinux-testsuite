#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

int main(void)
{
	void *ptr;
	int rc;
	int pagesize = getpagesize();

	rc = posix_memalign(&ptr, pagesize, pagesize);
	if (rc) {
		fprintf(stderr, "posix_memalign failed: %d\n", rc);
		exit(1);
	}

	rc = mprotect(ptr, 4096, PROT_READ | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}


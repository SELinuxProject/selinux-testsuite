#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <malloc.h>

#ifndef DEFAULT_MMAP_THRESHOLD_MAX
#define DEFAULT_MMAP_THRESHOLD_MAX 512*1024
#endif

int main(void)
{
	void *ptr;
	int rc;
	int pagesize = getpagesize();

	rc = mallopt(M_MMAP_THRESHOLD, DEFAULT_MMAP_THRESHOLD_MAX);
	if (rc != 1) {
		fprintf(stderr, "mallopt failed: %d\n", rc);
		exit(1);
	}

	rc = posix_memalign(&ptr, pagesize, pagesize);
	if (rc) {
		fprintf(stderr, "posix_memalign failed: %d\n", rc);
		exit(1);
	}

	rc = mprotect(ptr, pagesize, PROT_READ | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}


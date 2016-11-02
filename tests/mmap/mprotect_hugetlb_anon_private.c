#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include "utils.h"

#ifndef MAP_HUGETLB
# define MAP_HUGETLB 0x40000
#endif

int main(void)
{
	char *ptr;
	int rc;
	long length = getdefaulthugesize();

	ptr = mmap(NULL, length, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1,
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

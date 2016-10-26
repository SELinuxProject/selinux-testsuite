#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

#ifndef MAP_HUGETLB
# define MAP_HUGETLB 0x40000
#endif

#define LENGTH (2*1024*1024)

int main(void)
{
	char *ptr;
	int rc;

	ptr = mmap(NULL, LENGTH, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1,
		   0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	rc = mprotect(ptr, LENGTH, PROT_READ | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}

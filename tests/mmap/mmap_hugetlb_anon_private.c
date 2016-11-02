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
	long length = getdefaulthugesize();

	ptr = mmap(NULL, length, PROT_READ | PROT_WRITE | PROT_EXEC,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	exit(0);
}

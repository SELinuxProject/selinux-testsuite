#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

int main(void)
{
	char *ptr;

	ptr = mmap(0, 4096, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	exit(0);
}


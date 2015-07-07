#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

int main(void)
{
	char *ptr;
	int rc;

	ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,
		   0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	rc = mprotect(ptr, 4096, PROT_READ | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}


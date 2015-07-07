#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>

int main(void)
{
	char buf[4096];
	int rc;
	void *ptr;
	long pagesize = sysconf(_SC_PAGESIZE);

	ptr = (void *) (((unsigned long) buf) & ~(pagesize - 1));

	rc = mprotect(ptr, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC);
	if (rc < 0) {
		perror("mprotect");
		exit(1);
	}
	exit(0);
}


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <pthread.h>

static void *test_thread(void *p)
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
	return NULL;
}

int main(int argc, char **argv)
{
	struct utsname uts;
	pthread_t thread;

	if (argc != 2) {
		fprintf(stderr, "usage: %s [pass|fail]\n", argv[0]);
		exit(1);
	}

	if (strcmp(argv[1], "pass") && strcmp(argv[1], "fail")) {
		fprintf(stderr, "usage: %s [pass|fail]\n", argv[0]);
		exit(1);
	}

	if (uname(&uts) < 0) {
		perror("uname");
		exit(1);
	}

	if (!strcmp(argv[1], "fail") && strverscmp(uts.release, "4.7") < 0) {
		printf("%s: Kernels < 4.7 do not check execstack on thread stacks, skipping.\n",
		       argv[0]);
		/* pass the test by failing as if it was denied */
		exit(1);
	}

	pthread_create(&thread, NULL, test_thread, NULL);
	pthread_join(thread, NULL);
	exit(0);
}


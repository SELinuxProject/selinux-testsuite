#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/syscall.h>

#ifndef __NR_memfd_secret
# define __NR_memfd_secret 447
#endif

#define TEXT "Hello World!\nHello World!\nHello World!\nHello World!\nHello World!\nHello World!\n"

static int _memfd_secret(unsigned long flags)
{
	return syscall(__NR_memfd_secret, flags);
}

int main(int argc, const char *argv[])
{
	long page_size;
	int fd, flags;
	char *mem;
	bool check = (argc == 2 && strcmp(argv[1], "check") == 0);
	bool wx = (argc == 2 && strcmp(argv[1], "wx") == 0);

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0) {
		fprintf(stderr, "failed to get pagesize, got %ld:  %s\n", page_size,
			strerror(errno));
		return EXIT_FAILURE;
	}

	fd = _memfd_secret(0);
	if (fd < 0) {
		printf("memfd_secret() failed:  %s\n", strerror(errno));
		if (check && errno != ENOSYS)
			return EXIT_SUCCESS;

		return EXIT_FAILURE;
	}

	if (check)
		return EXIT_SUCCESS;

	if (ftruncate(fd, page_size) < 0) {
		fprintf(stderr, "ftruncate failed:  %s\n", strerror(errno));
	}

	flags = PROT_READ | PROT_WRITE;
	if (wx)
		flags |= PROT_EXEC;

	mem = mmap(NULL, page_size, flags, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED || !mem) {
		printf("unable to mmap secret memory:  %s\n", strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}

	close(fd);

	memcpy(mem, TEXT, sizeof(TEXT));

	if (memcmp(mem, TEXT, sizeof(TEXT)) != 0) {
		fprintf(stderr, "data not synced\n");
		munmap(mem, page_size);
		return EXIT_FAILURE;
	}

	munmap(mem, page_size);

	return EXIT_SUCCESS;
}

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int fd;
	char *name = "mymemfd";

	fd = memfd_create(name, 0);
	if (fd < 0) {
		perror("memfd_create");
		exit(-1);
	}

	close(fd);
	exit(0);
}

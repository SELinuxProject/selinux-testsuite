#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int memfd_fd, exec_fd;
	int len, written, rc;
	char *name = "mymemfd";

	if (argc != 2) {
		printf("Usage: %s <fexec_binary>\n", argv[0]);
		exit(-1);
	}

	memfd_fd = memfd_create(name, 0);
	if (memfd_fd < 0) {
		perror("memfd_create");
		exit(-1);
	}

	exec_fd = open(argv[1], O_RDONLY);
	if (exec_fd < 0) {
		perror("open");
		exit(-1);
	}

	char buf[8192];
	while ((len = read(exec_fd, buf, sizeof(buf))) > 0) {
		written = write(memfd_fd, buf, len);
		if (len != written) {
			perror("read/write");
			exit(-1);
		}
	}
	close(exec_fd);

	char *empty_env[] = {NULL};
	char *nothing_argv[] = {argv[1], NULL};

	rc = fexecve(memfd_fd, nothing_argv, empty_env);

	perror("fexecve");
	exit(rc);
}

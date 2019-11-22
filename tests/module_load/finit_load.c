#define _GNU_SOURCE 1

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/syscall.h>
#include <selinux/selinux.h>

static void print_usage(char *progfile_name)
{
	fprintf(stderr,
		"usage:  %s [-v] path name\n"
		"Where:\n\t"
		"-v    Print information.\n\t"
		"path  Kernel module build path.\n\t"
		"name  Name of kernel module to load.\n", progfile_name);
	exit(-1);
}

int main(int argc, char *argv[])
{
	char *context, file_name[PATH_MAX];
	int opt, result, fd, s_errno;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (optind >= argc)
		print_usage(argv[0]);

	result = sprintf(file_name, "%s/%s.ko", argv[optind],
			 argv[optind + 1]);
	if (result < 0) {
		fprintf(stderr, "Failed sprintf\n");
		exit(-1);
	}

	fd = open(file_name, O_RDONLY);
	if (!fd) {
		fprintf(stderr, "Failed to open %s: %s\n",
			file_name, strerror(errno));
		exit(-1);
	}

	if (verbose) {
		result = getcon(&context);
		if (result < 0) {
			fprintf(stderr, "Failed to obtain process context\n");
			close(fd);
			exit(-1);
		}

		printf("Process context:\n\t%s\n", context);
		free(context);
	}

	result = syscall(__NR_finit_module, fd, "", 0);
	s_errno = errno;
	close(fd);
	if (result < 0) {
		fprintf(stderr, "Failed to load '%s' module: %s\n",
			file_name, strerror(s_errno));
		/* Denying: sys_module=EPERM, module_load=EACCES */
		exit(s_errno);
	}

	if (verbose)
		printf("Loaded kernel module:  %s\n", file_name);

	result = syscall(__NR_delete_module, argv[optind + 1], 0);
	if (result < 0) {
		fprintf(stderr, "Failed to delete '%s' module: %s\n",
			argv[optind + 1], strerror(errno));
		exit(-1);
	}

	if (verbose)
		printf("Deleted kernel module: %s\n", argv[optind + 1]);

	return 0;
}

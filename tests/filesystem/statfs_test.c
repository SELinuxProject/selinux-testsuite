#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/statfs.h>
#include <selinux/selinux.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -t path [-v]\n"
		"Where:\n\t"
		"-t  Target path to statfs(2)\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, save_err;
	char *context, *tgt = NULL;
	bool verbose = false;
	struct statfs statfs_t;

	while ((opt = getopt(argc, argv, "t:v")) != -1) {
		switch (opt) {
		case 't':
			tgt = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!tgt)
		print_usage(argv[0]);

	if (verbose) {
		result = getcon(&context);
		if (result < 0) {
			fprintf(stderr, "Failed to obtain process context\n");
			return -1;
		}
		printf("Process context:\n\t%s\n", context);
		free(context);
	}

	result = statfs(tgt, &statfs_t);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "statfs(2) Failed: %s\n", strerror(errno));
		return save_err;
	}

	if (verbose)
		printf("statfs(2) returned magic filesystem: 0x%lx\n",
		       statfs_t.f_type);

	return 0;
}

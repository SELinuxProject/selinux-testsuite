#include "fs_common.h"

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -t tgt -o options [-v]\n"
		"Where:\n\t"
		"-t  Target path\n\t"
		"-f  Filesystem type\n\t"
		"-o  Options list (comma separated list)\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, pfd;
	char *context, *tgt = NULL, *opts = NULL;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "t:o:v")) != -1) {
		switch (opt) {
		case 't':
			tgt = optarg;
			break;
		case 'o':
			opts = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!tgt || !opts)
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

	if (verbose)
		printf("Mounting\n\ttgt: %s\n\topts: %s\n", tgt, opts);

	pfd = fspick(AT_FDCWD, tgt, 0);
	if (pfd < 0) {
		fprintf(stderr, "Failed fspick(2): %s\n", strerror(errno));
		return -1;
	}

	/* config_opts() will return 0 or errno from fsconfig(2) */
	result = fsconfig_opts(pfd, NULL, tgt, opts, verbose);
	if (result) {
		fprintf(stderr, "Failed config_opts\n");
		return result;
	}

	close(pfd);
	return 0;
}

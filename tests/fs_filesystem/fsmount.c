#include "fs_common.h"

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -s src -t tgt -f fs_type [-o options] [-v]\n"
		"Where:\n\t"
		"-s  Source path\n\t"
		"-t  Target path\n\t"
		"-f  Filesystem type\n\t"
		"-o  Options list (comma separated list)\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, fsfd, mfd;
	char *context, *src = NULL, *tgt = NULL, *fs_type = NULL, *opts = NULL;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "s:t:f:o:v")) != -1) {
		switch (opt) {
		case 's':
			src = optarg;
			break;
		case 't':
			tgt = optarg;
			break;
		case 'f':
			fs_type = optarg;
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

	if (!src || !tgt || !fs_type)
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
		printf("Mounting\n\tsrc: %s\n\ttgt: %s\n\tfs_type: %s\n\topts: %s\n",
		       src, tgt, fs_type, opts);

	fsfd = fsopen(fs_type, 0);
	if (fsfd < 0) {
		fprintf(stderr, "Failed fsopen(2): %s\n", strerror(errno));
		return -1;
	}

	/* config_opts() will return 0 or errno from fsconfig(2) */
	result = fsconfig_opts(fsfd, src, NULL, opts, verbose);
	if (result) {
		fprintf(stderr, "Failed config_opts\n");
		return result;
	}

	mfd = fsmount(fsfd, 0, 0);
	if (mfd < 0) {
		fprintf(stderr, "Failed fsmount(2): %s\n", strerror(errno));
		return -1;
	}
	close(fsfd);

	result = move_mount(mfd, "", AT_FDCWD, tgt, MOVE_MOUNT_F_EMPTY_PATH);
	if (result < 0) {
		fprintf(stderr, "Failed move_mount(2): %s\n", strerror(errno));
		return -1;
	}
	close(mfd);

	return 0;
}

#include "fs_common.h"

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -s src -t tgt [-v]\n"
		"Where:\n\t"
		"-s  Source path\n\t"
		"-t  Target path\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, fd1, fd2, save_errno;
	char *context, *src = NULL, *tgt = NULL;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "s:t:v")) != -1) {
		switch (opt) {
		case 's':
			src = optarg;
			break;
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

	if (!src || !tgt)
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
		printf("Mounting\n\tsrc: %s\n\ttgt: %s\n", src, tgt);

	fd1 = open_tree(AT_FDCWD, src, 0);
	if (fd1 < 0) {
		fprintf(stderr, "Failed fd1 open_tree(2): %s\n",
			strerror(errno));
		return -1;
	}

	fd2 = open_tree(fd1, "", AT_EMPTY_PATH | OPEN_TREE_CLONE | AT_RECURSIVE);
	if (fd2 < 0) {
		fprintf(stderr, "Failed fd2 open_tree(2): %s\n",
			strerror(errno));
		return -1;
	}

	result = move_mount(fd2, "", AT_FDCWD, tgt, MOVE_MOUNT_F_EMPTY_PATH);
	save_errno = errno;
	if (result < 0) {
		fprintf(stderr, "Failed move_mount(2): %s\n", strerror(errno));
		return save_errno;
	}

	close(fd1);
	close(fd2);
	return 0;
}

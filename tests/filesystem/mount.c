#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <selinux/selinux.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-s src] -t tgt [-f fs_type] [-o options] [-bmprv]\n"
		"Where:\n\t"
		"-s  Source path\n\t"
		"-t  Target path\n\t"
		"-f  Filesystem type\n\t"
		"-o  Options list (comma separated list)\n\t"
		"Zero or one of the following flags:\n\t"
		"\t-b  MS_BIND\n\t"
		"\t-m  MS_MOVE\n\t"
		"\t-p  MS_PRIVATE\n\t"
		"\t-r  MS_REMOUNT\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

static int ck_mount(char *mntpoint)
{
	int result = 0;
	FILE *fp;
	size_t len;
	ssize_t num;
	char *buf = NULL;

	fp = fopen("/proc/mounts", "re");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open /proc/mounts: %s\n",
			strerror(errno));
		return -1;
	}

	while ((num = getline(&buf, &len, fp)) != -1) {
		if (strstr(buf, mntpoint) != NULL) {
			result = 1;
			break;
		}
	}

	free(buf);
	fclose(fp);
	return result;
}

int main(int argc, char *argv[])
{
	int opt, result, save_err, flags = 0;
	char *context, *src = NULL, *tgt = NULL, *fs_type = NULL, *opts = NULL;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "s:t:f:o:pbmrv")) != -1) {
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
		case 'b':
			flags = MS_BIND;
			break;
		case 'p':
			flags = MS_PRIVATE;
			break;
		case 'm':
			flags = MS_MOVE;
			break;
		case 'r':
			flags = MS_REMOUNT;
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

	if (verbose)
		printf("Mounting\n\tsrc: %s\n\ttgt: %s\n\tfs_type: %s flags: 0x%x\n\topts: %s\n",
		       src, tgt, fs_type, flags, opts);

	result = mount(src, tgt, fs_type, flags, opts);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "Failed mount(2): %s\n", strerror(errno));
		return save_err;
	}

	if (flags == MS_MOVE) {
		if (!ck_mount(src) && ck_mount(tgt)) {
			if (verbose)
				printf("MS_MOVE: Moved mountpoint\n");
		} else {
			fprintf(stderr, "MS_MOVE: Move mountpoint failed\n");
			return -1;
		}
	}

	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/fanotify.h>
#include <selinux/selinux.h>

#ifndef FAN_MARK_FILESYSTEM
#define FAN_MARK_FILESYSTEM	0x00000100
#endif
#ifndef FAN_MARK_MOUNT
#define FAN_MARK_MOUNT		0x00000010
#endif

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -t path [-m] [-v]\n"
		"Where:\n\t"
		"-t  Target mount point to mark\n\t"
		"-m  Set flag to FAN_MARK_MOUNT, default is FAN_MARK_FILESYSTEM\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int mask = FAN_OPEN, flags;
	int fd, result, opt, save_err;
	char *context, *tgt = NULL;
	bool verbose = false;

	flags = 0;

	while ((opt = getopt(argc, argv, "t:mv")) != -1) {
		switch (opt) {
		case 't':
			tgt = optarg;
			break;
		case 'm':
			flags = FAN_MARK_ADD | FAN_MARK_MOUNT;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!flags)
		flags = FAN_MARK_ADD | FAN_MARK_FILESYSTEM;

	if (!tgt)
		print_usage(argv[0]);

	if (verbose) {
		result = getcon(&context);
		if (result < 0) {
			fprintf(stderr, "Failed to obtain process context\n");
			exit(-1);
		}
		printf("Process context:\n\t%s\n", context);
		free(context);
	}

	fd = fanotify_init(FAN_CLASS_CONTENT, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "fanotify_init(2) Failed: %s\n",
			strerror(errno));
		exit(-1);
	}

	result = fanotify_mark(fd, flags, mask, AT_FDCWD, tgt);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "fanotify_mark(2) Failed: %s\n",
			strerror(errno));
		close(fd);
		return save_err;
	}

	if (verbose)
		printf("Set fanotify_mark(2) on filesystem: %s\n", tgt);

	close(fd);
	return 0;
}

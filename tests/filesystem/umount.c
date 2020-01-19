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
		"usage:  %s [-t] [-v]\n"
		"Where:\n\t"
		"-t  Target path\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

#define WAIT_COUNT 60
#define USLEEP_TIME 100000

int main(int argc, char *argv[])
{
	char *context, *tgt = NULL;
	int opt, result, i, save_err;
	bool verbose = false;

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
			exit(-1);
		}
		printf("Process context:\n\t%s\n", context);
		free(context);
	}

	/*
	 * umount(2) will sometimes return EBUSY when other tasks are
	 * checking mounts so wait around before bailing out.
	 */
	for (i = 0; i < WAIT_COUNT; i++) {
		result = umount(tgt);
		save_err = errno;
		if (!result) {
			if (verbose)
				printf("Unmounted: %s\n", tgt);

			return 0;
		}

		if (verbose && save_err == EBUSY)
			printf("umount(2) returned EBUSY %d times\n", i + 1);

		if (save_err != EBUSY) {
			fprintf(stderr, "Failed umount(2): %s\n",
				strerror(save_err));
			return save_err;
		}
		usleep(USLEEP_TIME);
	}

	fprintf(stderr, "Failed to umount(2) after %d retries with: %s\n",
		WAIT_COUNT, strerror(save_err));

	return save_err;
}

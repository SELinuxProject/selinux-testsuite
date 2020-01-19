#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <linux/unistd.h>
#include <selinux/selinux.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -f file -e cxt [-v]\n"
		"Where:\n\t"
		"-f  File to check its context\n\t"
		"-e  Expected context\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, result, fd;
	char *context = NULL, *expected = NULL, *file = NULL;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "f:e:v")) != -1) {
		switch (opt) {
		case 'f':
			file = optarg;
			break;
		case 'e':
			expected = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!file || !expected)
		print_usage(argv[0]);

	fd = open(file, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open(2) Failed: %s\n", strerror(errno));
		return -1;
	}

	result = fgetfilecon(fd, &context);
	if (result < 0) {
		fprintf(stderr, "fgetfilecon(3) Failed: %s\n",
			strerror(errno));
		result = -1;
		goto err;
	}
	result = 0;

	if (strcmp(expected, context)) {
		fprintf(stderr, "File context error, expected:\n\t%s\ngot:\n\t%s\n",
			expected, context);
		result = -1;
	} else {
		if (verbose)
			printf("Pass - File contexts match: %s\n", context);
	}
err:
	free(context);
	close(fd);

	return result;
}

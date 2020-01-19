#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <linux/unistd.h>
#include <selinux/selinux.h>
#include <selinux/context.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -m mp [-e ctx] [-r] [-v]\n"
		"Where:\n\t"
		"-m  Mount point\n\t"
		"-e  Expected MP context\n\t"
		"-r  Reset MP context to 'unlabeled_t'\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, result;
	char *context = NULL, *expected = NULL, *mount = NULL, *newcon = NULL;
	bool verbose = false, reset = false;
	const char *type = "unlabeled_t";
	context_t con_t;

	while ((opt = getopt(argc, argv, "m:e:rv")) != -1) {
		switch (opt) {
		case 'm':
			mount = optarg;
			break;
		case 'e':
			expected = optarg;
			break;
		case 'r':
			reset = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!mount)
		print_usage(argv[0]);

	result = getfilecon(mount, &context);
	if (result < 0) {
		fprintf(stderr, "getfilecon(3) Failed: %s\n",
			strerror(errno));
		result = -1;
		goto err;
	}
	if (verbose)
		printf("Current MP context: %s\n", context);

	result = 0;

	if (reset) {
		/* Set context to unlabeled_t */
		con_t = context_new(context);
		if (!con_t) {
			fprintf(stderr, "Unable to create context structure\n");
			result = -1;
			goto err;
		}

		if (context_type_set(con_t, type)) {
			fprintf(stderr, "Unable to set new type\n");
			free(con_t);
			result = -1;
			goto err;
		}

		newcon = context_str(con_t);
		free(con_t);
		if (!newcon) {
			fprintf(stderr, "Unable to obtain new context string\n");
			result = -1;
			goto err;
		}

		result = setfilecon(mount, newcon);
		free(newcon);
		if (result < 0) {
			fprintf(stderr, "setfilecon(3) Failed: %s\n",
				strerror(errno));
			result = -1;
			goto err;
		}

		free(context);

		result = getfilecon(mount, &context);
		if (result < 0) {
			fprintf(stderr, "getfilecon(3) Failed: %s\n",
				strerror(errno));
			return result;
		}
		result = 0;

		if (verbose)
			printf("Set new MP context: %s\n", context);

	} else {
		if (strcmp(expected, context)) {
			fprintf(stderr, "Mount context error, expected:\n\t%s\ngot:\n\t%s\n",
				expected, context);
			result = -1;
		} else {
			if (verbose)
				printf("Pass - Mountpoint contexts match: %s\n",
				       context);
		}
	}

err:
	free(context);
	return result;
}

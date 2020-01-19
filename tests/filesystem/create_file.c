#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <selinux/selinux.h>
#include <selinux/context.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -f file [-e type] [-v]\n"
		"Where:\n\t"
		"-f  File to create\n\t"
		"-e  Expected file context type\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, result, fd, save_err;
	char *context, *file = NULL, *type = NULL;
	const char *file_type;
	bool verbose = false;
	context_t con_t;

	while ((opt = getopt(argc, argv, "f:e:v")) != -1) {
		switch (opt) {
		case 'f':
			file = optarg;
			break;
		case 'e':
			type = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!file)
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

	fd = creat(file, O_RDWR);
	save_err = errno;
	if (fd < 0) {
		fprintf(stderr, "creat(2) Failed: %s\n", strerror(errno));
		return save_err;
	}

	if (verbose)
		printf("Created file: %s\n", file);

	context = NULL;
	result = fgetfilecon(fd, &context);
	if (result < 0) {
		fprintf(stderr, "fgetfilecon(3) Failed: %s\n",
			strerror(errno));
		result = -1;
		goto err;
	}
	result = 0;

	if (verbose)
		printf("File context: %s\n", context);

	if (type) {
		con_t = context_new(context);
		if (!con_t) {
			fprintf(stderr, "Unable to create context structure\n");
			result = -1;
			goto err;
		}

		file_type = context_type_get(con_t);
		if (!file_type) {
			fprintf(stderr, "Unable to get new type\n");
			free(con_t);
			result = -1;
			goto err;
		}

		if (strcmp(type, file_type)) {
			fprintf(stderr, "File context error, expected:\n\t%s\ngot:\n\t%s\n",
				type, file_type);
			result = -1;
		} else {
			result = 0;
			if (verbose)
				printf("File context is correct\n");
		}
		free(con_t);
	}

err:
	free(context);
	close(fd);

	return result;
}

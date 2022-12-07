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
		"usage:  %s -f file -t type [-v]\n"
		"Where:\n\t"
		"-f  File to create\n\t"
		"-t  Type for context of created file\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, result, fd, save_err;
	const char *newfcon;
	char *orgfcon, *type = NULL, *file = NULL;
	char *context;
	bool verbose = false;
	context_t con_t;

	while ((opt = getopt(argc, argv, "f:t:v")) != -1) {
		switch (opt) {
		case 'f':
			file = optarg;
			break;
		case 't':
			type = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!type || !file)
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

	/* hooks.c may_create() FILESYSTEM__ASSOCIATE */
	fd = creat(file, O_RDWR);
	save_err = errno;
	if (fd < 0) {
		fprintf(stderr, "creat(2) Failed: %s\n", strerror(save_err));
		return save_err;
	}
	if (verbose)
		printf("Created file: %s\n", file);

	result = fgetfilecon(fd, &orgfcon);
	if (result < 0) {
		fprintf(stderr, "fgetfilecon(3) Failed: %s\n",
			strerror(errno));
		close(fd);
		return -1;
	}
	if (verbose)
		printf("Current file context is: %s\n", orgfcon);

	/* Build new file context */
	con_t = context_new(orgfcon);
	freecon(orgfcon);
	if (!con_t) {
		fprintf(stderr, "Unable to create context structure\n");
		close(fd);
		return -1;
	}

	if (context_type_set(con_t, type)) {
		fprintf(stderr, "Unable to set new type\n");
		context_free(con_t);
		close(fd);
		return -1;
	}

	newfcon = context_str(con_t);
	if (!newfcon) {
		fprintf(stderr, "Unable to obtain new context string\n");
		context_free(con_t);
		close(fd);
		return -1;
	}

	/* hooks.c selinux_inode_setxattr() FILESYSTEM__ASSOCIATE */
	result = fsetfilecon(fd, newfcon);
	save_err = errno;
	close(fd);
	if (result < 0) {
		fprintf(stderr, "fsetfilecon(3) Failed: %s\n",
			strerror(save_err));
		context_free(con_t);
		return save_err;
	}

	fd = open(file, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open(2) Failed: %s\n", strerror(errno));
		context_free(con_t);
		return -1;
	}

	result = fgetfilecon(fd, &context);
	if (result < 0) {
		fprintf(stderr, "fgetfilecon(3) Failed: %s\n",
			strerror(errno));
		close(fd);
		context_free(con_t);
		return -1;
	}
	if (verbose)
		printf("New file context is: %s\n", context);

	close(fd);

	result = 0;
	if (strcmp(newfcon, context)) {
		fprintf(stderr, "File context error, expected:\n\t%s\ngot:\n\t%s\n",
			newfcon, context);
		result = -1;
	}

	context_free(con_t);
	return result;
}

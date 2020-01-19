#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <linux/limits.h>
#include <selinux/selinux.h>
#include <selinux/context.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -b path -t type [-v]\n"
		"Where:\n\t"
		"-b  base directory\n\t"
		"-t  Type for fs context to generate on base/mp1\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, result, save_err;
	char *context, *fs_con = NULL, *newcon = NULL, *base_dir, *type;
	char fs_mount[PATH_MAX];
	bool verbose = false;
	context_t con_t;

	while ((opt = getopt(argc, argv, "b:t:v")) != -1) {
		switch (opt) {
		case 'b':
			base_dir = optarg;
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

	if (!type || !base_dir)
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

	result = getfilecon(base_dir, &context);
	if (result < 0) {
		fprintf(stderr, "getfilecon(3) Failed: %s\n",
			strerror(errno));
		return -1;
	}
	if (verbose)
		printf("%s context is: %s\n", base_dir, context);

	/* Build new context for fs */
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

	result = setfscreatecon(newcon);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "Failed setfscreatecon(3): %s\n",
			strerror(errno));
		result = save_err;
		goto err;
	}

	sprintf(fs_mount, "%s/%s", base_dir, "mp1");

	result = mkdir(fs_mount, 0777);
	if (result < 0) {
		fprintf(stderr, "mkdir(2) Failed: %s\n", strerror(errno));
		goto err;
	}

	result = getfilecon(fs_mount, &fs_con);
	if (result < 0) {
		fprintf(stderr, "getfilecon(3) Failed: %s\n",
			strerror(errno));
		goto err;
	}
	if (verbose)
		printf("%s context is: %s\n", fs_mount, fs_con);

	result = rmdir(fs_mount);
	if (result < 0) {
		fprintf(stderr, "rmdir(2) Failed: %s\n", strerror(errno));
		goto err;
	}

	if (strcmp(newcon, fs_con)) {
		fprintf(stderr, "FS context error, expected:\n\t%s\ngot:\n\t%s\n",
			newcon, fs_con);
		result = -1;
	}
err:
	free(context);
	free(newcon);
	free(fs_con);

	return result;
}

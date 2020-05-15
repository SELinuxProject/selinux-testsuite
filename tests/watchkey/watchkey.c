/* Based on kernel source samples/watch_queue/watch_test.c */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/watch_queue.h>
#include <linux/unistd.h>
#include <linux/keyctl.h>
#include <selinux/selinux.h>

#define BUF_SIZE 256

/* Require syscall() as function not yet in libkeyutils */
static long keyctl_watch_key(int key, int watch_fd, int watch_id)
{
	return syscall(__NR_keyctl, KEYCTL_WATCH_KEY, key, watch_fd, watch_id);
}

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v]\n"
		"Where:\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, fd, pipefd[2], result, save_errno;
	char *context;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (verbose) {
		result = getcon(&context);
		if (result < 0) {
			fprintf(stderr, "Failed to obtain process context\n");
			exit(-1);
		}

		printf("Process context:\n\t%s\n", context);
		free(context);
	}

	result = pipe2(pipefd, O_NOTIFICATION_PIPE);
	if (result < 0) {
		fprintf(stderr, "Failed to create pipe2(2): %s\n",
			strerror(errno));
		exit(-1);
	}
	fd = pipefd[0];

	result = ioctl(fd, IOC_WATCH_QUEUE_SET_SIZE, BUF_SIZE);
	if (result < 0) {
		fprintf(stderr, "Failed to set watch_queue size: %s\n",
			strerror(errno));
		exit(-1);
	}

	save_errno = 0;
	/* Requires key { view } */
	result = keyctl_watch_key(KEY_SPEC_PROCESS_KEYRING, fd,
				  WATCH_TYPE_KEY_NOTIFY);
	if (result < 0) {
		save_errno = errno;
		fprintf(stderr, "Failed keyctl_watch_key(): %s\n",
			strerror(errno));
		goto err;
	}
	if (verbose)
		printf("keyctl_watch_key() successful\n");

err:
	close(fd);
	return save_errno;
}

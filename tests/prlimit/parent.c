#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <selinux/selinux.h>
#include <selinux/context.h>

void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-g] [-s soft|hard] newdomain program\n",
		progname);
	exit(-1);
}

#define RESOURCE RLIMIT_NOFILE

int main(int argc, char **argv)
{
	char buf[1];
	int pid, rc, fd[2], fd2[2], opt;
	security_context_t context_s;
	context_t context;
	struct rlimit newrlim, oldrlim, *newrlimp = NULL, *oldrlimp = NULL;
	bool get = false, set = false, soft = false;

	while ((opt = getopt(argc, argv, "gs:")) != -1) {
		switch (opt) {
		case 'g':
			get = true;
			break;
		case 's':
			set = true;
			if (!strcasecmp(optarg, "soft"))
				soft = true;
			else if (!strcasecmp(optarg, "hard"))
				soft = false;
			else
				usage(argv[0]);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (!get && !set) {
		usage(argv[0]);
		exit(-1);
	}

	if ((argc - optind) != 2) {
		usage(argv[0]);
		exit(-1);
	}

	rc = getcon(&context_s);
	if (rc < 0) {
		fprintf(stderr, "%s:  unable to get my context\n", argv[0]);
		exit(-1);

	}

	context = context_new(context_s);
	if (!context) {
		fprintf(stderr, "%s:  unable to create context structure\n", argv[0]);
		exit(-1);
	}

	if (context_type_set(context, argv[optind])) {
		fprintf(stderr, "%s:  unable to set new type\n", argv[0]);
		exit(-1);
	}

	freecon(context_s);
	context_s = context_str(context);
	if (!context_s) {
		fprintf(stderr, "%s:  unable to obtain new context string\n", argv[0]);
		exit(-1);
	}

	rc = setexeccon(context_s);
	if (rc < 0) {
		fprintf(stderr, "%s:  unable to set exec context to %s\n", argv[0], context_s);
		exit(-1);
	}

	rc = getrlimit(RESOURCE, &oldrlim);
	if (rc < 0) {
		perror("getrlimit");
		exit(-1);
	}

	rc = pipe(fd);
	if (rc < 0) {
		perror("pipe");
		exit(-1);
	}

	rc = pipe(fd2);
	if (rc < 0) {
		perror("pipe");
		exit(-1);
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(-1);
	} else if (pid == 0) {
		dup2(fd[0], 0);
		dup2(fd2[1], 1);
		execv(argv[optind + 1], argv + optind + 1);
		buf[0] = -1;
		write(1, buf, 1);
		close(1);
		perror(argv[optind + 1]);
		exit(-1);
	}

	rc = read(fd2[0], buf, 1);
	if (rc < 0) {
		perror("read");
		exit(-1);
	}

	if (get)
		oldrlimp = &oldrlim;

	if (set) {
		newrlimp = &newrlim;
		if (soft) {
			newrlim.rlim_max = oldrlim.rlim_max;
			if (newrlim.rlim_cur == RLIM_INFINITY)
				newrlim.rlim_cur = 1024;
			else
				newrlim.rlim_cur = oldrlim.rlim_cur / 2;
		} else {
			newrlim.rlim_cur = oldrlim.rlim_cur;
			if (newrlim.rlim_max == RLIM_INFINITY)
				newrlim.rlim_max = 1024;
			else
				newrlim.rlim_max = oldrlim.rlim_max / 2;
		}
	}

	rc =  prlimit(pid, RESOURCE, newrlimp, oldrlimp);
	if (rc < 0) {
		perror("prlimit");
		write(fd[1], buf, 1);
		close(fd[1]);
		exit(1);
	}

	write(fd[1], buf, 1);
	close(fd[1]);
	exit(0);
}

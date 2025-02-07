#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <sched.h>

int clone_fn(void *arg)
{
	char **argv = arg;
	execv(argv[3], argv + 3);
	perror(argv[3]);
	return -1;
}

int main(int argc, char **argv)
{
	int pagesize;
	void *clone_stack, *page;
	int pid, rc, status, cloneflags;
	const char *context_s;
	char *context_tmp;
	context_t context;

	if (argc != 4) {
		fprintf(stderr, "usage:  %s cloneflags newdomain program\n", argv[0]);
		exit(-1);
	}

	cloneflags = strtol(argv[1], NULL, 0);
	if (!cloneflags) {
		fprintf(stderr, "invalid clone flags %s\n", argv[1]);
		exit(-1);
	}

	pagesize = 16 * getpagesize();
	page = malloc(pagesize);
	if (!page) {
		perror("malloc");
		exit(-1);
	}
	clone_stack = (unsigned char *)page + pagesize;

	rc = getcon(&context_tmp);
	if (rc < 0) {
		fprintf(stderr, "%s:  unable to get my context\n", argv[0]);
		exit(-1);

	}

	context = context_new(context_tmp);
	if (!context) {
		fprintf(stderr, "%s:  unable to create context structure\n", argv[0]);
		exit(-1);
	}

	if (context_type_set(context, argv[2])) {
		fprintf(stderr, "%s:  unable to set new type\n", argv[0]);
		exit(-1);
	}

	freecon(context_tmp);
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

	pid = clone(clone_fn, clone_stack, cloneflags | SIGCHLD, argv);
	if (pid < 0) {
		perror("clone");
		exit(-1);
	}

	pid = wait(&status);
	if (pid < 0) {
		perror("wait");
		exit(-1);
	}

	if (WIFEXITED(status)) {
		fprintf(stderr, "Child exited with status %d.\n", WEXITSTATUS(status));
		exit(WEXITSTATUS(status));
	}

	if (WIFSTOPPED(status)) {
		fprintf(stderr, "Child stopped by signal %d.\n", WSTOPSIG(status));
		fprintf(stderr, "..This shouldn't happen.\n");
		fprintf(stderr, "..Killing the child.\n");
		rc = kill(pid, SIGKILL);
		if (rc < 0) {
			perror("kill");
			exit(-1);
		}
		exit(-1);
	}

	if (WIFSIGNALED(status)) {
		fprintf(stderr, "Child terminated by signal %d.\n", WTERMSIG(status));
		fprintf(stderr,
			"..This is consistent with a share permission denial, check the audit message.\n");
		exit(1);
	}

	fprintf(stderr, "Unexpected exit status 0x%x\n", status);
	exit(-1);
}


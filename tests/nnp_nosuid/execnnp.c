#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>

static void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-n] command [args...]\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	bool nobounded;
	struct utsname uts;
	pid_t pid;
	int rc, status;
	int opt;
	bool nnp = false;

	while ((opt = getopt(argc, argv, "n")) != -1) {
		switch (opt) {
		case 'n':
			nnp = true;
			break;
		default:
			usage(argv[0]);
			break;
		}
	}

	if ((argc - optind) < 2)
		usage(argv[0]);

	if (uname(&uts) < 0) {
		perror("uname");
		exit(-1);
	}

	nobounded = ((strcmp(argv[argc - 1], "test_bounded_t") == 0) &&
		     (strverscmp(uts.release, "3.18") < 0));

	if (nnp) {
		rc = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
		if (rc < 0) {
			perror("prctl PR_SET_NO_NEW_PRIVS");
			exit(-1);
		}
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(-1);
	}

	if (pid == 0) {
		execvp(argv[optind], &argv[optind]);
		perror(argv[optind]);
		exit(-1);
	}

	pid = wait(&status);
	if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) && nobounded) {
			printf("%s:  Kernels < v3.18 do not support bounded transitions under NNP.\n",
			       argv[0]);
			/* pass the test */
			exit(0);
		}
		exit(WEXITSTATUS(status));
	}

	fprintf(stderr, "Unexpected exit status 0x%x\n", status);
	exit(-1);
}

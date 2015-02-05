#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/prctl.h>

int main(int argc, char **argv)
{
	bool nobounded;
	struct utsname uts;
	pid_t pid;
	int rc, status;

	if (argc < 2) {
		fprintf(stderr, "usage:  %s command [args...]\n", argv[0]);
		exit(-1);
	}

	if (uname(&uts) < 0) {
		perror("uname");
		exit(-1);
	}

	nobounded = ((strcmp(argv[argc-1], "test_nnp_bounded_t") == 0) &&
		     (strverscmp(uts.release, "3.18") < 0));

	rc = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	if (rc < 0) {
		perror("prctl PR_SET_NO_NEW_PRIVS");
		exit(-1);
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(-1);
	}

	if (pid == 0) {
		execvp(argv[1], &argv[1]);
		perror(argv[1]);
		exit(-1);
	}

	pid = wait(&status);
	if (WIFEXITED(status)) {
		if (nobounded) {
			if (!WEXITSTATUS(status))
				exit(-1);
			printf("%s:  Kernels < v3.18 do not support bounded transitions under NNP.\n", argv[0]);
			/* pass the test */
			exit(0);
		}
		exit(WEXITSTATUS(status));
	}

	fprintf(stderr, "Unexpected exit status 0x%x\n", status);
	exit(-1);
}

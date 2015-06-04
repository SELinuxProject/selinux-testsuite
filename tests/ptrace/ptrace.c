#include<stdio.h>
#include<stdlib.h>
#include<sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
	int rc;
	int pid;
	pid = atoi(argv[1]);

	rc = ptrace(PTRACE_ATTACH, pid, 0, 0);

	if(rc < 0) {
		exit(1);
	} else {
		wait(NULL);
		rc = ptrace(PTRACE_DETACH, pid, 0, 0);
		if (rc < 0) {
			perror("PTRACE_DETACH");
			exit(1);
		}
	}
	exit(0);
}


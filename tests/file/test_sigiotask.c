#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <string.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/file.h>
#include <sys/ioctl.h>
#include<fcntl.h>
#include<unistd.h>
#include<signal.h>
#include<asm/ioctls.h>
#include <libgen.h>
#include <pty.h>

/*
 * Test the sigio operations by creating a child and registering that process
 * for SIGIO signals for the terminal. The main process forces a SIGIO
 * on the terminal by sending a charcter to that device.
 */
int main(int argc, char **argv)
{

	int fd, pipefd[2];
	int rc;
	int flags;
	pid_t pid;
	char key = '\r';
	char buf[1];

	/*
	 * ctermid returns controlling terminal, which could be console, pts,..
	 * It may not be present in some situations, e.g. running in automated test
	 * environment, where init/service spawning this test has not ctty:
	 * if (fork() > 0) {
	 *   _exit(0);
	 * }
	 * setsid();
	 */
	pid_t ret;
	int master, slave;

	ret = openpty(&master, &slave, NULL, NULL, NULL);
	if (ret == -1) {
		perror("test_sigiotask:openpty");
		exit(2);
	}
	fd = slave;

	rc = pipe(pipefd);
	if (rc == -1) {
		perror("test_sigiotask:pipe");
		exit(2);
	}

	/*
	 * Spawn off the child process to handle the information protocol.
	 */
	if((pid = fork()) < 0 ) {
		perror("test_sigiotask:fork");
		exit(2);
	}

	/*
	 * child process
	 */
	if( pid == 0 ) {
		char ex_name[255];
		char pipefd_str[16];
		/* Create the path to the executable the child will run */
		sprintf(ex_name, "%s/wait_io", dirname(strdup(argv[0])));
		sprintf(pipefd_str, "%i", pipefd[1]);
		if( execl(ex_name, "wait_io", pipefd_str, (char *) 0) < 0 ) {
			perror("test_sigiotask:execl");
			exit(2);
		}
	}

	/* Wait for the child to start up.
	 * If the fcntls below occurs before child sets up its signal handler
	 * and there is some new data on tty then it will die by SIGIO.
	 * Example 1: fd is /dev/console and kernel prints message to it
	 * Example 2: if you run it through ptrace, ptrace will print to the same fd
	 */
	rc = read(pipefd[0], buf, 1);
	if( rc == -1 ) {
		perror("test_sigiotask:read");
		exit(2);
	}

	/*
	 * parent process
	 */
	rc = fcntl(fd, F_SETSIG, 0);
	if( rc == -1 ) {
		perror("test_sigiotask:F_SETSIG");
		exit(2);
	}

	rc = fcntl(fd, F_SETOWN, pid);
	if( rc == -1 ) {
		perror("test_sigiotask:F_SETOWN");
		exit(2);
	}

	flags = fcntl(fd, F_GETFL, 0);
	if( flags < 0 ) {
		perror("test_sigiotask:F_GETFL");
		exit(2);
	}
	flags |= O_ASYNC;
	rc = fcntl(fd, F_SETFL, flags);
	if( rc == -1 ) {
		perror("test_sigiotask:F_SETFL");
		exit(2);
	}

	rc = ioctl(fd, TIOCSTI, &key);  /* Send a key to the tty device */
	if( rc == -1 ) {
		perror("test_sigiotask:write");
		exit(2);
	}
	close(fd);
	wait(&rc);
	if( WIFEXITED(rc) ) {   /* exit status from child is normal? */
		printf("%s:  child exited OK %d\n", argv[0], WIFEXITED(rc));
		printf("%s:  exiting with %d\n", argv[0], WEXITSTATUS(rc));
		exit(WEXITSTATUS(rc));
	} else {
		printf("%s:  error exit\n", argv[0]);
		exit(1);
	}

}

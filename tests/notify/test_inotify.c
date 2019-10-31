#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <getopt.h>

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: test_inotify [-r] file_name\n");
		exit(1);
	}

	int fd, wd, arg;
	int mask = IN_MODIFY;

	while ((arg = getopt(argc, argv, "pr")) != -1) {
		switch (arg) {
		case 'r':
			mask |= IN_ACCESS;
			break;
		default:
			fprintf(stderr, "Usage: test_inotify [-r] file_name\n");
			exit(1);
		}
	}

	// get new file descriptor for inotify access
	fd = inotify_init();
	if (fd < 0) {
		perror("inotify_init:bad file descriptor");
		exit(1);
	}

	// set watch on file and get watch descriptor for accessing events on it
	wd = inotify_add_watch(fd, argv[optind], mask);

	if (wd < 0) {
		perror("test_inotify:watch denied");
		exit(1);
	}

	exit(0);
}

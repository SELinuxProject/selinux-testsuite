#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>

#include <getopt.h>

#include <fcntl.h>
#include <poll.h>
#include <sys/fanotify.h>
#include <unistd.h>

void printUsage()
{
	fprintf(stderr, "Usage: test_fanotify [-p] [-r] [-l] [-m] file_name\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printUsage();
	}

	int fd, ret, arg;
	int mask = FAN_OPEN;  // default mask
	int flags = FAN_MARK_ADD;
	int listening = 0;

	// the -p flag will test for watch_with_perm
	// the mask used at mark will contain FAN_OPEN_PERM
	//
	// the -r flag will test for watching accesses to files for reads
	// the mask will contain FAN_ACCESS
	while ((arg = getopt(argc, argv, "prlm")) != -1) {
		switch (arg) {
		case 'p':
			mask |= FAN_OPEN_PERM;
			break;
		case 'r':
			mask |= FAN_ACCESS;
			break;
		case 'l':
			listening = 1;
			break;
		case 'm':
			flags |= FAN_MARK_MOUNT;
			break;
		default:
			printUsage();
		}
	}

	// get file descriptor for new fanotify event queue
	fd = fanotify_init(FAN_CLASS_CONTENT, O_RDWR);
	if (fd < 0) {
		perror("fanotify_init:bad file descriptor");
		exit(1);
	}

	// mark a filesystem object and add mark to event queue
	// get notifications on file opens, accesses, and closes
	// use current working directory as base dir
	ret = fanotify_mark(fd, flags, mask, AT_FDCWD, argv[optind]);

	if (ret < 0) {
		perror("test_fanotify:watch denied");
		exit(1);
	}

	// logic to actually listen for an event if the -l flag is passed
	// this is used to test if an app with read-only access can get a read/write
	// handle to the watched file
	if (listening) {
		if (fork() == 0) {  // fork a child process to cause an event on the file
			FILE *f;

			f = fopen(argv[optind], "r");  // open file for reading
			fgetc(f);                      // read char from file

			fclose(f);
		} else {  // logic to watch for events and try to access file read/write
			struct pollfd fds;
			fds.fd = fd;
			fds.events = POLLIN;

			while (listening) {
				int polled = poll(&fds, 1, 1);
				if (polled > 0) {
					if (fds.revents & POLLIN) {
						struct fanotify_event_metadata buff[200];

						size_t len = read(fd, (void *)&buff, sizeof(buff));
						if (len == -1) {
							perror("test_fanotify:can't open file");
							exit(1);
						} else {
							listening = 0;
							break;
						}
					}
				} else if (polled == -1) {
					listening = 0;
				}
			}
		}
	}
	exit(0);
}

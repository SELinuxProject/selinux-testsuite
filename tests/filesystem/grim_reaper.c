#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <selinux/selinux.h>

#define WAIT_COUNT 60
#define USLEEP_TIME 10000

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -t tgt [-v]\n"
		"Where:\n\t"
		"-t  target to unmount if active\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	FILE *fp;
	size_t len;
	ssize_t num;
	int opt, index = 0, i, result = 0;
	char *mount_info[2], *buf = NULL, *item, *tgt;
	bool verbose = false;

	while ((opt = getopt(argc, argv, "t:v")) != -1) {
		switch (opt) {
		case 't':
			tgt = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!tgt)
		print_usage(argv[0]);

	fp = fopen("/proc/mounts", "re");
	if (!fp) {
		fprintf(stderr, "Failed to open /proc/mounts: %s\n",
			strerror(errno));
		return -1;
	}

	while ((num = getline(&buf, &len, fp)) != -1) {
		index = 0;
		item = strtok(buf, " ");
		while (item != NULL) {
			mount_info[index] = item;
			index++;
			if (index == 2)
				break;
			item = strtok(NULL, " ");
		}

		if (strcmp(mount_info[0], tgt) == 0) {
			if (verbose)
				printf("Unmounting %s from %s\n",
				       mount_info[1], mount_info[0]);

			for (i = 0; i < WAIT_COUNT; i++) {
				result = umount(mount_info[1]);
				if (!result)
					break;

				if (errno != EBUSY) {
					fprintf(stderr, "Failed umount(2): %s\n",
						strerror(errno));
					break;
				}
				usleep(USLEEP_TIME);
			}
		}
	}

	free(buf);
	fclose(fp);
	return result;
}

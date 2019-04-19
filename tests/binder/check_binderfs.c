#include "binder_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v]\n"
		"Where:\n\t"
		"-v Print new device information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, fd, result;
	size_t len;
	struct binderfs_device device = { 0 };

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	len = strlen(BINDERFS_NAME);
	memcpy(device.name, BINDERFS_NAME, len);

	fd = open(BINDERFS_CONTROL, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Failed to open binder-control device: %s\n",
			strerror(errno));
		return 1;
	}

	result = ioctl(fd, BINDER_CTL_ADD, &device);
	if (result < 0) {
		fprintf(stderr, "Failed to allocate new binder device: %s\n",
			strerror(errno));
		result = 2;
		goto brexit;
	}

	if (verbose)
		printf("Allocated new binder device: major %d minor %d"
		       " with name \"%s\"\n", device.major, device.minor,
		       device.name);
brexit:
	close(fd);
	return result;
}

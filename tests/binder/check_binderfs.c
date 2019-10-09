#include "binder_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v]\n"
		"Where:\n\t"
		"-v Print status information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, control_fd, dev_fd, result;
	size_t len;
	char dev_str[128];
	struct binder_version vers;
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

	control_fd = open(BINDERFS_CONTROL, O_RDONLY | O_CLOEXEC);
	if (control_fd < 0) {
		fprintf(stderr, "Failed to open binder-control device: %s\n",
			strerror(errno));
		return NO_BINDER_SUPPORT;
	}

	result = ioctl(control_fd, BINDER_CTL_ADD, &device);
	if (result < 0) {
		fprintf(stderr, "Failed to allocate new binder device: %s\n",
			strerror(errno));
		result = BINDER_ERROR;
		goto brexit;
	}

	if (verbose)
		printf("Allocated new binder device: major %d minor %d"
		       " with name \"%s\"\n", device.major, device.minor,
		       device.name);

	result = sprintf(dev_str, "%s/%s", BINDERFS_DEV, BINDERFS_NAME);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain Binder dev name\n");
		result = BINDER_ERROR;
		goto brexit;
	}

	dev_fd = open(dev_str, O_RDWR | O_CLOEXEC);
	if (dev_fd < 0) {
		fprintf(stderr, "Cannot open: %s error: %s\n", dev_str,
			strerror(errno));
		result = BINDER_ERROR;
		goto brexit;
	}

	result = ioctl(dev_fd, BINDER_VERSION, &vers);
	if (result < 0) {
		fprintf(stderr, "ioctl BINDER_VERSION: %s\n",
			strerror(errno));
		result = BINDER_ERROR;
		goto brexit;
	}
	close(dev_fd);

	if (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION) {
		fprintf(stderr,
			"Binder kernel version: %d differs from user space version: %d\n",
			vers.protocol_version,
			BINDER_CURRENT_PROTOCOL_VERSION);
		result = BINDER_VER_ERROR;
		goto brexit;
	}
	if (verbose)
		printf("Binder kernel version: %d\n", vers.protocol_version);

	result = BINDERFS_SUPPORT;

brexit:
	close(control_fd);
	return result;
}

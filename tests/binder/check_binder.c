#include "binder_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v]\n"
		"Where:\n\t"
		"-v Print binder version.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, result, fd;
	struct binder_version vers;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	fd = open(BINDER_DEV, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Cannot open: %s error: %s\n",
			BINDER_DEV, strerror(errno));
		return NO_BINDER_SUPPORT;
	}

	result = ioctl(fd, BINDER_VERSION, &vers);
	if (result < 0) {
		fprintf(stderr, "ioctl BINDER_VERSION: %s\n",
			strerror(errno));
		result = BINDER_ERROR;
		goto brexit;
	}

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

	result = BASE_BINDER_SUPPORT;

brexit:
	close(fd);

	return result;
}

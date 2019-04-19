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
	void *mapped;
	size_t mapsize = BINDER_MMAP_SIZE;
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
		result = 1;
		return result;
	}

	/* Need this or 'no VMA error' from kernel */
	mapped = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mapped == MAP_FAILED) {
		fprintf(stderr, "mmap error: %s\n", strerror(errno));
		close(fd);
		exit(-1);
	}

	result = ioctl(fd, BINDER_VERSION, &vers);
	if (result < 0) {
		fprintf(stderr, "ioctl BINDER_VERSION: %s\n",
			strerror(errno));
		goto brexit;
	}

	if (vers.protocol_version != BINDER_CURRENT_PROTOCOL_VERSION) {
		fprintf(stderr,
			"Binder kernel version: %d differs from user space version: %d\n",
			vers.protocol_version,
			BINDER_CURRENT_PROTOCOL_VERSION);
		result = 2;
		goto brexit;
	}

	if (verbose)
		printf("Binder kernel version: %d\n", vers.protocol_version);

brexit:
	munmap(mapped, mapsize);
	close(fd);

	return result;
}

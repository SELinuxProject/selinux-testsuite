#include "bpf_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -m|-p [-v]\n"
		"Where:\n\t"
		"-m    Create BPF map fd\n\t"
		"-p    Create BPF prog fd\n\t"
		"-v Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, fd;
	bool verbose = false;
	char *context;

	enum {
		MAP_FD = 1,
		PROG_FD
	} bpf_fd_type;

	while ((opt = getopt(argc, argv, "mpv")) != -1) {
		switch (opt) {
		case 'm':
			bpf_fd_type = MAP_FD;
			break;
		case 'p':
			bpf_fd_type = PROG_FD;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain SELinux context\n");
		exit(-1);
	}

	if (verbose)
		printf("Process context:\n\t%s\n", context);

	free(context);

	/* If BPF enabled, then need to set limits */
	bpf_setrlimit();

	switch (bpf_fd_type) {
	case MAP_FD:
		if (verbose)
			printf("Creating BPF map\n");

		fd = create_bpf_map();
		break;
	case PROG_FD:
		if (verbose)
			printf("Creating BPF prog\n");

		fd = create_bpf_prog();
		break;
	default:
		usage(argv[0]);
	}

	if (fd < 0)
		return fd;

	close(fd);
	return 0;
}

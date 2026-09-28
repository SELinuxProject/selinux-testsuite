#include "bpf_common.h"

#define write_verbose(verbose, fmt, ...) \
	do { \
		if (verbose) \
			printf(fmt "\n", ##__VA_ARGS__); \
	} while (0)

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -m|-p|-c|-l|-s|-f [-v]\n"
		"Where:\n\t"
		"-m    Create BPF map fd\n\t"
		"-p    Create BPF prog fd\n\t"
		"-c    Test BPF token map create\n\t"
		"-l    Test BPF token program load\n\t"
		"-s    Test BPF token cross-domain SUCCESS (exec-based)\n\t"
		"-f    Test BPF token cross-domain FAILURE (exec-based)\n\t"
		"-v Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, ret;
	bool verbose = false, is_fd = true;
	char *context;

	enum {
		MAP_FD = 1,
		PROG_FD,
		MAP_CREATE,
		PROG_LOAD,
		TOKEN_CROSS_DOMAIN_SUCCESS,
		TOKEN_CROSS_DOMAIN_FAILURE,
	} bpf_fd_type = -1;

	while ((opt = getopt(argc, argv, "mpclvsf")) != -1) {
		switch (opt) {
		case 'm':
			bpf_fd_type = MAP_FD;
			break;
		case 'p':
			bpf_fd_type = PROG_FD;
			break;
		case 'c':
			bpf_fd_type = MAP_CREATE;
			break;
		case 'l':
			bpf_fd_type = PROG_LOAD;
			break;
		case 's':
			bpf_fd_type = TOKEN_CROSS_DOMAIN_SUCCESS;
			break;
		case 'f':
			bpf_fd_type = TOKEN_CROSS_DOMAIN_FAILURE;
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

	write_verbose(verbose, "Process context:\n\n%s", context);

	/* Set environment variable for child helper to find itself */
	setenv("TEST_BASEDIR", dirname(strdup(argv[0])), 1);

	free(context);

	/* If BPF enabled, then need to set limits */
	bpf_setrlimit();

	switch (bpf_fd_type) {
	case MAP_FD:
		write_verbose(verbose, "Creating BPF map");

		ret = create_bpf_map();
		break;
	case PROG_FD:
		write_verbose(verbose, "Creating BPF prog");

		ret = create_bpf_prog();
		break;
	case MAP_CREATE:
		is_fd = false;
		write_verbose(verbose, "Testing BPF map create");

		ret = test_bpf_map_create();
		break;
	case PROG_LOAD:
		is_fd = false;
		write_verbose(verbose, "Testing BPF prog load");

		ret = test_bpf_prog_load();
		break;
	case TOKEN_CROSS_DOMAIN_SUCCESS:
		is_fd = false;
		write_verbose(verbose, "Testing BPF token cross-domain SUCCESS (exec)");

		ret = test_bpf_token_cap_cross_success();
		break;
	case TOKEN_CROSS_DOMAIN_FAILURE:
		is_fd = false;
		write_verbose(verbose, "Testing BPF token cross-domain FAILURE (exec)");

		ret = test_bpf_token_cap_cross_failure();
		break;
	default:
		usage(argv[0]);
	}

	if (ret < 0)
		return ret;

	if (is_fd)
		close(ret);

	return 0;
}

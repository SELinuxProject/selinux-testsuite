#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <keyutils.h>
#include <selinux/selinux.h>

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] newcon\n"
		"Where:\n\t"
		"-v      Print information.\n\t"
		"newcon  New keyring context.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result;
	char *context, *keycreate_con;
	char r_con[256];
	bool verbose = false;
	key_serial_t newring;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (optind >= argc)
		usage(argv[0]);

	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain process context\n");
		exit(1);
	}
	if (verbose)
		printf("Process context: %s\n", context);
	free(context);

	result = setkeycreatecon(argv[optind]);
	if (result < 0) {
		fprintf(stderr, "Failed setkeycreatecon(): %s\n",
			strerror(errno));
		exit(2);
	}

	result = getkeycreatecon(&keycreate_con);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain keycreate context\n");
		exit(3);
	}
	if (verbose)
		printf("New keycreate context: %s\n", keycreate_con);
	free(keycreate_con);

	newring = add_key("keyring", "my-keyring", NULL, 0,
			  KEY_SPEC_PROCESS_KEYRING);
	if (newring < 0) {
		fprintf(stderr, "Failed to add new keyring: %s\n",
			strerror(errno));
		exit(4);
	}

	result = keyctl(KEYCTL_GET_SECURITY, newring, r_con, sizeof(r_con));
	if (result < 0) {
		fprintf(stderr, "Failed to obtain key context: %s\n",
			strerror(errno));
		exit(5);
	}

	if (strcmp(argv[optind], r_con)) {
		fprintf(stderr, "Relabel error - expected: %s got: %s\n",
			argv[optind], r_con);
		exit(6);
	}

	if (verbose) {
		printf("'my-keyring' key ID: 0x%x\n", newring);
		printf("'my-keyring' context:\n\t%s\n", r_con);
	}

	return 0;
}

#include "keys_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v]\n"
		"Where:\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, nr, result;
	unsigned int timeout = 5;
	char *context, *keycreate_con, type[20], desc[30];
	char r_con[256];
	bool verbose = false;
	key_serial_t retrieved, search, link, compute,
		     private, prime, base, test_key;
	struct keyctl_dh_params params;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain process context\n");
		exit(1);
	}
	if (verbose)
		printf("Process context:\n\t%s\n", context);

	result = getkeycreatecon(&keycreate_con);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain keycreate context\n");
		exit(2);
	}
	if (verbose)
		printf("Current keycreate context:\n\t%s\n", keycreate_con);
	free(keycreate_con);

	/* Set context requires key { create } and process { setkeycreate } */
	result = setkeycreatecon(context);
	if (result < 0) {
		fprintf(stderr, "Failed setkeycreatecon(): %s\n",
			strerror(errno));
		exit(3);
	}
	if (verbose)
		printf("Set keycreate context:\n\t%s\n", context);
	free(context);

	result = getkeycreatecon(&keycreate_con);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain keycreate context\n");
		exit(4);
	}
	if (verbose)
		printf("New keycreate context:\n\t%s\n", keycreate_con);
	free(keycreate_con);

	/*
	 * Add three keys to the KEY_SPEC_PROCESS_KEYRING as these will be
	 * required by the keyctl(KEYCTL_DH_COMPUTE, ..) function.
	 * These require key { create write } permissions.
	 */
	private = add_key("user", "private", payload, strlen(payload),
			  KEY_SPEC_PROCESS_KEYRING);
	if (private < 0) {
		fprintf(stderr, "Failed add_key(private): %s\n",
			strerror(errno));
		exit(5);
	}

	prime = add_key("user", "prime", payload, strlen(payload),
			KEY_SPEC_PROCESS_KEYRING);
	if (prime < 0) {
		fprintf(stderr, "Failed add_key(prime): %s\n",
			strerror(errno));
		exit(5);
	}

	base = add_key("user", "base", payload, strlen(payload),
		       KEY_SPEC_PROCESS_KEYRING);
	if (base < 0) {
		fprintf(stderr, "Failed add_key(base): %s\n",
			strerror(errno));
		exit(5);
	}

	if (verbose) {
		printf("Private key ID: 0x%x\n", private);
		printf("Prime key ID:   0x%x\n", prime);
		printf("Base key ID:    0x%x\n", base);
	}

	/* Requires key { search }. From kernel 5.3 requires { link } */
	retrieved = request_key("user", "private", NULL,
				KEY_SPEC_PROCESS_KEYRING);
	if (retrieved < 0) {
		fprintf(stderr, "Failed to request 'private' key: %s\n",
			strerror(errno));
		exit(6);
	}

	/* Requires key { search } */
	search = keyctl(KEYCTL_SEARCH, KEY_SPEC_PROCESS_KEYRING,
			"user", "base", 0);
	if (search < 0) {
		fprintf(stderr, "Failed to find 'base' key: %s\n",
			strerror(errno));
		exit(7);
	}

	/* Requires key { view } */
	result = keyctl(KEYCTL_GET_SECURITY, private, r_con, sizeof(r_con));
	if (result < 0) {
		fprintf(stderr, "Failed to obtain 'private' key context: %s\n",
			strerror(errno));
		exit(8);
	}

	if (verbose) {
		printf("Requested 'private' key ID: 0x%x\n", retrieved);
		printf("Searched 'base' key ID:     0x%x\n", search);
		printf("Searched 'base' key context:\n\t%s\n", r_con);
	}

	/* Compute DH key, only obtain the length for test, not the key. */
	params.priv = private;
	params.prime = prime;
	params.base = base;

	/* Requires key { create read write } */
	compute = keyctl(KEYCTL_DH_COMPUTE, &params, NULL, 0, 0);
	if (compute < 0) {
		fprintf(stderr, "Failed KEYCTL_DH_COMPUTE: %s\n",
			strerror(errno));
		exit(9);
	}
	if (verbose)
		printf("KEYCTL_DH_COMPUTE key ID size: %d\n", compute);

	/* Requires key { write link } */
	link = keyctl(KEYCTL_LINK, base, KEY_SPEC_PROCESS_KEYRING);
	if (link < 0) {
		fprintf(stderr, "Failed KEYCTL_LINK: %s\n",
			strerror(errno));
		exit(10);
	}
	if (verbose)
		printf("Link key ID:    0x%x\n", KEY_SPEC_PROCESS_KEYRING);

	/* Requires key { setattr } */
	link = keyctl(KEYCTL_SET_TIMEOUT, base, timeout);
	if (link < 0) {
		fprintf(stderr, "Failed KEYCTL_SET_TIMEOUT: %s\n",
			strerror(errno));
		exit(11);
	}
	if (verbose) {
		test_key = keyctl(KEYCTL_DESCRIBE, base, r_con, sizeof(r_con));
		if (test_key < 0) {
			fprintf(stderr, "Failed KEYCTL_DESCRIBE: %s\n",
				strerror(errno));
			exit(11);
		}
		result = sscanf(r_con, "%[^;];%d;%d;%x;%s",
				type, &nr, &nr, &nr, desc);
		if (result < 0) {
			fprintf(stderr, "Failed sscanf(): %s\n",
				strerror(errno));
			exit(11);
		}
		printf("Set %d second timeout on key Type: '%s' Description: '%s'\n",
		       timeout, type, desc);
	}

	return 0;
}

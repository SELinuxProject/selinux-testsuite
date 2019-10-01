#include "keys_common.h"

int main(int argc, char **argv)
{
	int result, nr;
	unsigned int timeout = 5;
	char r_con[512], type[20], desc[30], *context;
	bool verbose = false;
	key_serial_t private, prime, base, compute, test_key;
	struct keyctl_dh_params params;

	/*
	 * There are two parameters passed:
	 *    1 - The security context for setcon(3)
	 *    2 - Verbose mode
	 */
	if (argv[2] != NULL)
		verbose = true;

	if (verbose)
		printf("%s process information:\n", argv[0]);

	/*
	 * Use setcon() as policy will not allow multiple type_transition
	 * statements using the same target with different process types.
	 */
	result = setcon(argv[1]);
	if (result < 0) {
		fprintf(stderr, "setcon() failed to set process context: %s\n",
			argv[1]);
		exit(1);
	}

	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain process context\n");
		exit(2);
	}
	if (verbose)
		printf("\tProcess context:\n\t\t%s\n", context);

	free(context);

	/*
	 * Join this session to the parent as ./keyring_service executed:
	 *    keyctl(KEYCTL_JOIN_SESSION_KEYRING, "test-session")
	 *
	 * Requires key { link }
	 */
	test_key = keyctl(KEYCTL_SESSION_TO_PARENT);
	if (test_key < 0) {
		fprintf(stderr, "Failed KEYCTL_SESSION_TO_PARENT: %s\n",
			strerror(errno));
		exit(3);
	}

	/* Requires key { view } */
	result = keyctl(KEYCTL_GET_SECURITY, KEY_SPEC_SESSION_KEYRING,
			r_con, sizeof(r_con));
	if (result < 0) {
		fprintf(stderr, "Failed to obtain parent session context: %s\n",
			strerror(errno));
		exit(4);
	}
	if (verbose)
		printf("\tJoined session to parent. Parent keyring context:\n\t\t%s\n",
		       r_con);

	/* Requires key { search write }. From kernel 5.3 requires { link } */
	private = request_key("user", "private", NULL,
			      KEY_SPEC_SESSION_KEYRING);
	if (private < 0) {
		fprintf(stderr, "Failed to request 'private' key: %s\n",
			strerror(errno));
		exit(5);
	}

	prime = request_key("user", "prime", NULL, KEY_SPEC_SESSION_KEYRING);
	if (prime < 0) {
		fprintf(stderr, "Failed to request 'prime' key: %s\n",
			strerror(errno));
		exit(5);
	}

	base = request_key("user", "base", NULL, KEY_SPEC_SESSION_KEYRING);
	if (base < 0) {
		fprintf(stderr, "Failed to request 'base' key: %s\n",
			strerror(errno));
		exit(5);
	}
	if (verbose) {
		printf("\tRequested 'private' key ID: 0x%x\n", private);
		printf("\tRequested 'prime'   key ID: 0x%x\n", prime);
		printf("\tRequested 'base'    key ID: 0x%x\n", base);
	}

	/* Requires key { setattr } */
	test_key = keyctl(KEYCTL_SET_TIMEOUT, base, timeout);
	if (test_key < 0) {
		fprintf(stderr, "Failed KEYCTL_SET_TIMEOUT: %s\n",
			strerror(errno));
		exit(6);
	}
	if (verbose) {
		test_key = keyctl(KEYCTL_DESCRIBE, base, r_con, sizeof(r_con));
		if (test_key < 0) {
			fprintf(stderr, "Failed KEYCTL_DESCRIBE: %s\n",
				strerror(errno));
			exit(7);
		}
		result = sscanf(r_con, "%[^;];%d;%d;%x;%s",
				type, &nr, &nr, &nr, desc);
		if (result < 0) {
			fprintf(stderr, "Failed sscanf(): %s\n",
				strerror(errno));
			exit(7);
		}
		printf("\tSet %d second timeout on key Type: '%s' Description: '%s'\n",
		       timeout, type, desc);
	}

	/* Compute DH key, only obtain the length for test, not the key. */
	params.priv = private;
	params.prime = prime;
	params.base = base;

	/* Requires key { read write } */
	compute = keyctl(KEYCTL_DH_COMPUTE, &params, NULL, 0, 0);
	if (compute < 0) {
		fprintf(stderr, "Failed KEYCTL_DH_COMPUTE: %s\n",
			strerror(errno));
		exit(8);
	}
	if (verbose)
		printf("\tKEYCTL_DH_COMPUTE key ID size: %d\n", compute);

	exit(0);
}

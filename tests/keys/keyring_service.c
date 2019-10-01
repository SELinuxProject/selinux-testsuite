#include "keys_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] newdomain program\n"
		"Where:\n\t"
		"newdomain  Type for new domain.\n\t"
		"program    Program to be exec'd.\n\t"
		"-v         Print information.\n", progname);
	exit(-1);
}

int main(int argc, char **argv)
{
	int opt, pid, result, status;
	bool verbose;
	char *context_s, *request_keys_argv[4] = { NULL };
	context_t context;
	key_serial_t private, prime, base, newring;

	verbose = false;

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			request_keys_argv[2] = strdup("-v");
			break;
		default:
			usage(argv[0]);
		}
	}

	if (argc - optind != 2)
		usage(argv[0]);

	if (verbose)
		printf("%s process information:\n", argv[0]);

	result = getcon(&context_s);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain process context\n");
		exit(1);
	}
	if (verbose)
		printf("\tProcess context:\n\t\t%s\n", context_s);

	/* Set context requires process { setkeycreate } and key { create } */
	result = setkeycreatecon(context_s);
	if (result < 0) {
		fprintf(stderr, "Failed setkeycreatecon(): %s\n",
			strerror(errno));
		exit(3);
	}
	if (verbose)
		printf("\tSet keycreate context:\n\t\t%s\n", context_s);

	context = context_new(context_s);
	if (!context) {
		fprintf(stderr, "Unable to create context structure\n");
		exit(2);
	}
	free(context_s);

	if (context_type_set(context, argv[optind])) {
		fprintf(stderr, "Unable to set new type\n");
		exit(3);
	}

	context_s = context_str(context);
	if (!context_s) {
		fprintf(stderr, "Unable to obtain new context string\n");
		exit(4);
	}
	if (verbose)
		printf("\t%s process context will transition to:\n\t\t%s\n",
		       argv[optind], context_s);

	/*
	 * This sets up the environment the for 'request_keys' service when it
	 * calls: keyctl(KEYCTL_SESSION_TO_PARENT)
	 */
	newring = keyctl(KEYCTL_JOIN_SESSION_KEYRING, "test-session");
	if (newring < 0) {
		fprintf(stderr, "Failed KEYCTL_JOIN_SESSION_KEYRING,: %s\n",
			strerror(errno));
		exit(5);
	}
	if (verbose)
		printf("\tKEYCTL_JOIN_SESSION_KEYRING newkey ID: 0x%x\n",
		       newring);

	private = add_key("user", "private", payload, strlen(payload),
			  KEY_SPEC_SESSION_KEYRING);
	if (private < 0) {
		fprintf(stderr, "Failed add_key(private): %s\n",
			strerror(errno));
		exit(6);
	}

	prime = add_key("user", "prime", payload, strlen(payload),
			KEY_SPEC_SESSION_KEYRING);
	if (prime < 0) {
		fprintf(stderr, "Failed add_key(prime): %s\n",
			strerror(errno));
		exit(6);
	}

	base = add_key("user", "base", payload, strlen(payload),
		       KEY_SPEC_SESSION_KEYRING);
	if (base < 0) {
		fprintf(stderr, "Failed add_key(base): %s\n",
			strerror(errno));
		exit(6);
	}
	if (verbose) {
		printf("\tAdded 'private' key ID: 0x%x\n", private);
		printf("\tAdded 'prime'   key ID: 0x%x\n", prime);
		printf("\tAdded 'base'    key ID: 0x%x\n", base);
	}

	pid = fork();
	if (pid < 0) {
		fprintf(stderr, "fork failed: %s\n", strerror(errno));
		exit(9);
	} else if (pid == 0) {
		signal(SIGTRAP, SIG_IGN);
		request_keys_argv[0] = strdup(argv[optind + 1]);
		request_keys_argv[1] = strdup(context_s);
		if (verbose)
			printf("\tExec parameters:\n\t\t%s\n\t\t%s\n\t\t%s\n",
			       request_keys_argv[0],
			       request_keys_argv[1],
			       request_keys_argv[2]);

		execv(request_keys_argv[0], request_keys_argv);
		fprintf(stderr, "execv of: %s failed: %s\n",
			request_keys_argv[0], strerror(errno));
		exit(10);
	}

	pid = wait(&status);
	if (pid < 0) {
		fprintf(stderr, "wait() failed: %s\n", strerror(errno));
		exit(11);
	}

	if (WIFEXITED(status)) {
		fprintf(stderr, "%s exited with status: %d\n",
			argv[optind + 1], WEXITSTATUS(status));
		exit(WEXITSTATUS(status));
	}

	if (WIFSIGNALED(status)) {
		fprintf(stderr, "%s terminated by signal: %d\n",
			argv[optind + 1], WTERMSIG(status));
		fprintf(stderr,
			"..This is consistent with a %s permission denial - check the audit log.\n",
			argv[optind + 1]);
		exit(12);
	}

	fprintf(stderr, "Unexpected exit status 0x%x\n", status);
	exit(-1);
}

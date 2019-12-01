#include "tun_common.h"

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v] newcon\n"
		"Where:\n\t"
		"-p      Test TAP driver, default is TUN driver.\n\t"
		"-v      Print information.\n\t"
		"newcon  New process context.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	char *origcon, *newcon, *test_str;
	char alloc_name[IFNAMSIZ];
	int opt, result, test, fd1, fd2;
	bool verbose = false;
	context_t con_t;
	struct ifreq ifr;

	test = IFF_TUN;
	test_str = "TUN";

	while ((opt = getopt(argc, argv, "pv")) != -1) {
		switch (opt) {
		case 'p':
			test = IFF_TAP;
			test_str = "TAP";
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (optind >= argc)
		print_usage(argv[0]);

	/*
	 * Need to keep a copy of this to switch back when testing deny
	 * relabelto/from in newcon. This will allow the removal of the
	 * allocated TUN/TAP name.
	 */
	result = getcon(&origcon);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain process context\n");
		exit(-1);
	}
	if (verbose)
		printf("Current process context:\n\t%s\n", origcon);

	/* Build new context for transition */
	con_t = context_new(origcon);
	if (!con_t) {
		fprintf(stderr, "Unable to create context structure\n");
		exit(-1);
	}

	if (context_type_set(con_t, argv[optind])) {
		fprintf(stderr, "Unable to set new type\n");
		exit(-1);
	}

	newcon = context_str(con_t);
	if (!newcon) {
		fprintf(stderr, "Unable to obtain new context string\n");
		exit(-1);
	}

	/* Start TUN/TAP */
	result = open_dev(&fd1, test_str, verbose);
	if (result != 0)
		exit(result);

	/* Let TUNSETIFF allocate the ifr_name for later use */
	memset(&ifr, 0, sizeof(struct ifreq));
	ifr.ifr_flags = test | IFF_MULTI_QUEUE;
	result = setiff(fd1, &ifr, verbose);
	if (result != 0)
		goto err1;

	strcpy(alloc_name, ifr.ifr_name);

	/* Make this name persist */
	result = persist(fd1, 1, alloc_name, verbose);
	if (result != 0)
		goto err1;

	/* Switch to new context for relabel tests */
	result = switch_context(newcon, verbose);
	if (result < 0) { /* If fail remove the persistent one */
		del_tuntap_name(fd1, origcon, alloc_name, verbose);
		goto err1;
	}

	/* Start TUN/TAP in new context */
	result = open_dev(&fd2, test_str, verbose);
	if (result != 0) {
		del_tuntap_name(fd1, origcon, alloc_name, verbose);
		goto err1;
	}

	memset(&ifr, 0, sizeof(struct ifreq));
	ifr.ifr_flags = test | IFF_MULTI_QUEUE;
	/* Use the TUN/TAP name allocated initially */
	strcpy(ifr.ifr_name, alloc_name);
	result = setiff(fd2, &ifr, verbose);
	if (result != 0) {
		del_tuntap_name(fd1, origcon, alloc_name, verbose);
		goto now_ok;
	}

	persist(fd2, 0, alloc_name, verbose);
now_ok:
	close(fd2);
err1:
	close(fd1);

	return result;
}

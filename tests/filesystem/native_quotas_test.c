#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/quota.h>
#include <xfs/xqm.h>
#include <selinux/selinux.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -s src [-v]\n"
		"Where:\n\t"
		"-s  Source\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, qcmd, save_err;
	char *context, *src = NULL;
	bool verbose = false;
	uint32_t fmtval;

	while ((opt = getopt(argc, argv, "s:v")) != -1) {
		switch (opt) {
		case 's':
			src = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!src)
		print_usage(argv[0]);

	if (verbose) {
		result = getcon(&context);
		if (result < 0) {
			fprintf(stderr, "Failed to obtain process context\n");
			return -1;
		}
		printf("Process context:\n\t%s\n", context);
		free(context);
	}

	/* This requires FILESYSTEM__QUOTAGET */
	qcmd = QCMD(Q_GETFMT, USRQUOTA);
	result = quotactl(qcmd, src, 0, (void *)&fmtval);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "quotactl(Q_GETFMT, USRQUOTA) Failed: %s\n",
			strerror(errno));
		return save_err;
	}
	if (verbose)
		printf("User Format: 0x%x\n", fmtval);

	/*
	 * The filesystem will be set up with quota on, therefore need to
	 * turn off then on.
	 * These require FILESYSTEM__QUOTAMOD
	 */
	qcmd = QCMD(Q_QUOTAOFF, USRQUOTA);
	result = quotactl(qcmd, src, 0, NULL);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "quotactl(Q_QUOTAOFF, USRQUOTA) Failed: %s\n",
			strerror(errno));
		return save_err;
	}
	if (verbose)
		printf("User Quota - OFF\n");

	qcmd = QCMD(Q_QUOTAON, USRQUOTA);
	result = quotactl(qcmd, src, 0, NULL);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "quotactl(Q_QUOTAON, USRQUOTA) Failed: %s\n",
			strerror(errno));
		return save_err;
	}
	if (verbose)
		printf("User Quota - ON\n");

	return 0;
}

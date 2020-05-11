#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/quota.h>
#include <selinux/selinux.h>

/*
 * This is required so that the code compiles on RHEL/CentOS 7 and below.
 * There <sys/quota.h> doesn't contain the definition and it conflicts
 * with <linux/quota.h>
 */
#ifndef QFMT_VFS_V0
#define QFMT_VFS_V0 2
#endif

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s -s src -t tgt [-v]\n"
		"Where:\n\t"
		"-s  Source path (e.g. /dev/loop0)\n\t"
		"-t  Target quota file (Full path with either 'aquota.user'\n\t"
		"    or 'aquota.group' appended)\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	int opt, result, qcmd, save_err, test_id = geteuid();
	char *context, *src = NULL, *tgt = NULL;
	bool verbose = false;
	uint32_t fmtval;

	while ((opt = getopt(argc, argv, "s:t:v")) != -1) {
		switch (opt) {
		case 's':
			src = optarg;
			break;
		case 't':
			tgt = optarg;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (!src || !tgt)
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

	if (strstr(tgt, "aquota.user") != NULL) {
		qcmd = QCMD(Q_QUOTAON, USRQUOTA);
		result = quotactl(qcmd, src, QFMT_VFS_V0, tgt);
		save_err = errno;
		if (result < 0) {
			fprintf(stderr, "quotactl(Q_QUOTAON, USRQUOTA) Failed: %s\n",
				strerror(errno));
			return save_err;
		}
		if (verbose)
			printf("User Quota - ON\n");

		qcmd = QCMD(Q_GETFMT, USRQUOTA);
		result = quotactl(qcmd, src, test_id, (caddr_t)&fmtval);
		save_err = errno;
		if (result < 0) {
			fprintf(stderr, "quotactl(Q_GETFMT, USRQUOTA) Failed: %s\n",
				strerror(errno));
			return save_err;
		}
		if (verbose)
			printf("User Format: 0x%x\n", fmtval);

		qcmd = QCMD(Q_QUOTAOFF, USRQUOTA);
		result = quotactl(qcmd, src, QFMT_VFS_V0, tgt);
		save_err = errno;
		if (result < 0) {
			fprintf(stderr, "quotactl(Q_QUOTAOFF, USRQUOTA) Failed: %s\n",
				strerror(errno));
			return save_err;
		}
		if (verbose)
			printf("User Quota - OFF\n");

		return 0;

	} else if (strstr(tgt, "aquota.group") != NULL) {
		qcmd = QCMD(Q_QUOTAON, GRPQUOTA);
		result = quotactl(qcmd, src, QFMT_VFS_V0, tgt);
		save_err = errno;
		if (result < 0) {
			fprintf(stderr, "quotactl(Q_QUOTAON, GRPQUOTA) Failed: %s\n",
				strerror(errno));
			return save_err;
		}
		if (verbose)
			printf("Group Quota - ON\n");

		qcmd = QCMD(Q_GETFMT, GRPQUOTA);
		result = quotactl(qcmd, src, test_id, (caddr_t)&fmtval);
		save_err = errno;
		if (result < 0) {
			fprintf(stderr, "quotactl(Q_GETFMT, GRPQUOTA) Failed: %s\n",
				strerror(errno));
			return save_err;
		}
		if (verbose)
			printf("Group Format: 0x%x\n", fmtval);

		qcmd = QCMD(Q_QUOTAOFF, GRPQUOTA);
		result = quotactl(qcmd, src, QFMT_VFS_V0, tgt);
		save_err = errno;
		if (result < 0) {
			fprintf(stderr, "quotactl(Q_QUOTAOFF, GRPQUOTA) Failed: %s\n",
				strerror(errno));
			return save_err;
		}
		if (verbose)
			printf("Group Quota - OFF\n");

		return 0;
	}

	fprintf(stderr, "Required %s to specify 'aquota.user' or 'aquota.group' file\n",
		tgt);
	return -1;
}

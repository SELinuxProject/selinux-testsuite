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
	int on_flags = XFS_QUOTA_UDQ_ACCT | XFS_QUOTA_UDQ_ENFD;
	int off_flags = XFS_QUOTA_UDQ_ENFD;
	struct fs_quota_stat q_stat;

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
	qcmd = QCMD(Q_XGETQSTAT, USRQUOTA);
	result = quotactl(qcmd, src, 0, (void *)&q_stat);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "quotactl(Q_XGETQSTAT, USRQUOTA) Failed: %s\n",
			strerror(errno));
		return save_err;
	}
	if (verbose)
		printf("XFS Q_XGETQSTAT Version: %d Flags: 0x%04x Number of dquots: %d\n",
		       q_stat.qs_version, q_stat.qs_flags, q_stat.qs_incoredqs);

	/*
	 * The tests turn XFS quotas on, therefore need to turn off then on
	 * These require FILESYSTEM__QUOTAMOD
	 */
	qcmd = QCMD(Q_XQUOTAOFF, USRQUOTA);
	result = quotactl(qcmd, src, 0, (void *)&off_flags);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "quotactl(Q_XQUOTAOFF, USRQUOTA) Failed: %s\n",
			strerror(errno));
		return save_err;
	}
	if (verbose)
		printf("XFS User Quota - OFF\n");

	qcmd = QCMD(Q_XQUOTAON, USRQUOTA);
	result = quotactl(qcmd, src, 0, (void *)&on_flags);
	save_err = errno;
	if (result < 0) {
		fprintf(stderr, "quotactl(Q_XQUOTAON, USRQUOTA) Failed: %s\n",
			strerror(errno));
		return save_err;
	}
	if (verbose)
		printf("XFS User Quota - ON\n");

	return 0;
}

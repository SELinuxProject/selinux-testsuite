#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <linux/pfkeyv2.h>
#include <selinux/selinux.h>

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-v]\n"
		"Where:\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	char *context;
	int opt, sock, result;
	bool verbose = false;
	struct timeval tm;
	struct sadb_msg w_msg, r_msg;
	int mlen = sizeof(struct sadb_msg);

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain process context\n");
		exit(-1);
	}

	if (verbose)
		printf("Process context:\n\t%s\n", context);

	free(context);

	sock = socket(PF_KEY, SOCK_RAW, PF_KEY_V2);
	if (sock < 0) {
		fprintf(stderr, "Failed to open key management socket: %s\n",
			strerror(errno));
		/* Return errno as denying net_admin=EPERM, create=EACCES */
		exit(errno);
	}

	if (verbose)
		printf("Opened key management socket\n");

	/* Set socket timeout for read in case no response from kernel */
	tm.tv_sec = 3;
	tm.tv_usec = 0;
	result = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tm, sizeof(tm));
	if (result < 0) {
		fprintf(stderr, "Failed setsockopt SO_RCVTIMEO: %s\n",
			strerror(errno));
		close(sock);
		exit(-1);
	}

	if (verbose)
		printf("setsocketopt: SO_RCVTIMEO - %ld seconds\n", tm.tv_sec);

	memset(&w_msg, 0, mlen);
	w_msg.sadb_msg_version = PF_KEY_V2;
	w_msg.sadb_msg_type = SADB_FLUSH;
	w_msg.sadb_msg_satype = SADB_SATYPE_AH;
	/* sadb_msg_len contains length in 64-bit words */
	w_msg.sadb_msg_len = (mlen / sizeof(uint64_t));
	w_msg.sadb_msg_seq = 99;
	w_msg.sadb_msg_pid = getpid();

	result = write(sock, &w_msg, mlen);
	if (result < 0) {
		fprintf(stderr, "Failed write to key management socket: %s\n",
			strerror(errno));
		close(sock);
		exit(errno); /* Return errno to test if EACCES */
	}

	if (verbose) {
		printf("Write sadb_msg data to key management socket:\n");
		printf("\tver: PF_KEY_V2 type: SADB_FLUSH sa_type: SADB_SATYPE_AH\n");
		printf("\tseq: %d pid: %d\n", w_msg.sadb_msg_seq,
		       w_msg.sadb_msg_pid);
	}

	memset(&r_msg, 0, mlen);

	result = read(sock, &r_msg, mlen);
	if (result < 0) {
		fprintf(stderr, "Failed to read key management socket: %s\n",
			strerror(errno));
		close(sock);
		exit(errno); /* Return errno to test if EACCES */
	}

	if (r_msg.sadb_msg_version != w_msg.sadb_msg_version ||
	    r_msg.sadb_msg_type != w_msg.sadb_msg_type ||
	    r_msg.sadb_msg_satype != w_msg.sadb_msg_satype ||
	    r_msg.sadb_msg_seq != w_msg.sadb_msg_seq ||
	    r_msg.sadb_msg_pid != getpid()) {
		fprintf(stderr, "Failed to read correct sadb_msg data:\n");
		fprintf(stderr, "\tSent - ver: %d type: %d sa_type: %d seq: %d pid: %d\n",
			w_msg.sadb_msg_version, w_msg.sadb_msg_type,
			w_msg.sadb_msg_satype, w_msg.sadb_msg_seq,
			w_msg.sadb_msg_pid);
		fprintf(stderr, "\tRecv - ver: %d type: %d sa_type: %d seq: %d pid: %d\n",
			r_msg.sadb_msg_version, r_msg.sadb_msg_type,
			r_msg.sadb_msg_satype, r_msg.sadb_msg_seq,
			r_msg.sadb_msg_pid);
		close(sock);
		exit(-1);
	}

	if (verbose) {
		printf("Read sadb_msg data from key management socket:\n");
		printf("\tver: PF_KEY_V2 type: SADB_FLUSH sa_type: SADB_SATYPE_AH\n");
		printf("\tseq: %d pid: %d\n", r_msg.sadb_msg_seq,
		       r_msg.sadb_msg_pid);
	}

	close(sock);
	return 0;
}

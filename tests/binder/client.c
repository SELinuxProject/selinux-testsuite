#include "binder_common.h"

static int binder_parse(int fd, binder_uintptr_t ptr, binder_size_t size);
static int transactions_complete;

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-c] [-n] [-r replies] [-m|-p] [-v]\n"
		"Where:\n\t"
		"-c  Use the number of replies for the BR_TRANSACTION_COMPLETE"
		" count.\n\t"
		"-n  Use the /dev/binderfs name service.\n\t"
		"-r  Number of replies expected that depends on the test.\n\t"
		"    It can be the number of BR_TRANSACTION_COMPLETE if\n\t"
		"    the -c option is set or number of times to issue the\n\t"
		"    ioctl - BINDER_WRITE_READ command if -c not set.\n\t"
		"-m  Service Provider sending BPF map fd.\n\t"
		"-p  Service Provider sending BPF prog fd.\n\t"
		"-v  Print context and command information.\n\t"
		"\nNote: Ensure this boolean command is run when "
		"testing after a reboot:\n\t"
		"setsebool allow_domain_fd_use=0\n", progname);
	exit(-1);
}

static void client_alarm(int sig)
{
	fprintf(stderr, "Client timer expired waiting for Binder reply\n");
	exit(-1);
}

/* This retrieves the Service Providers file descriptor, then using this
 * sends a simple transaction to trigger the impersonate permission.
 */
static void extract_fd_and_respond(const struct binder_transaction_data *txn_in)
{
	int result;
	uint32_t cmd;
	struct stat sb;
	struct binder_write_read bwr;
	const struct binder_fd_object *obj;
	struct {
		uint32_t cmd;
		struct binder_transaction_data txn;
	} __attribute__((packed)) writebuf;
	unsigned int readbuf[32];

	binder_size_t *offs = (binder_size_t *)
			      (binder_uintptr_t)txn_in->data.ptr.offsets;

	/* Get the binder_fd_object that contains the Service Providers
	 * binder file descriptor.
	 */
	obj = (const struct binder_fd_object *)
	      (((char *)(binder_uintptr_t)txn_in->data.ptr.buffer) + *offs);

	if (obj->hdr.type != BINDER_TYPE_FD) {
		fprintf(stderr, "Header not BINDER_TYPE_FD: %d\n",
			obj->hdr.type);
		exit(130);
	}

	/* fstat this just to see if a valid fd */
	result = fstat(obj->fd, &sb);
	if (result < 0) {
		fprintf(stderr, "Not a valid fd: %s\n", strerror(errno));
		exit(131);
	}

	if (verbose)
		printf("Client retrieved %s fd: %d st_dev: %ld\n",
		       fd_type_str, obj->fd, sb.st_dev);

	/* If testing BPF, then cannot do impersonate check */
	if (fd_type > BINDER_FD)
		return;

	memset(&writebuf, 0, sizeof(writebuf));
	memset(readbuf, 0, sizeof(readbuf));
	memset(&bwr, 0, sizeof(bwr));

	/* Send response using Service Providers fd to trigger
	 * impersonate check.
	 */
	writebuf.cmd = BC_TRANSACTION;
	writebuf.txn.target.handle = txn_in->target.handle;
	writebuf.txn.cookie = txn_in->cookie;
	writebuf.txn.code = txn_in->code;
	writebuf.txn.flags = TF_ONE_WAY;

	writebuf.txn.data_size = 0;
	writebuf.txn.data.ptr.buffer = (binder_uintptr_t)NULL;
	writebuf.txn.data.ptr.offsets = (binder_uintptr_t)NULL;
	writebuf.txn.offsets_size = 0;

	bwr.write_size = sizeof(writebuf);
	bwr.write_consumed = 0;
	bwr.write_buffer = (binder_uintptr_t)&writebuf;
	bwr.read_size = sizeof(readbuf);
	bwr.read_consumed = 0;
	bwr.read_buffer = (binder_uintptr_t)readbuf;

	result = ioctl(obj->fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr,
			"Client ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		exit(132);
	}

	if (verbose)
		printf("Client read_consumed: %lld\n",
		       bwr.read_consumed);

	cmd = binder_parse(obj->fd, (binder_uintptr_t)readbuf,
			   bwr.read_consumed);

	if (cmd == BR_FAILED_REPLY ||
	    cmd == BR_DEAD_REPLY ||
	    cmd == BR_DEAD_BINDER) {
		fprintf(stderr, "Client %s() failing command %s, exiting.\n",
			__func__, cmd_name(cmd));
		exit(133);
	}

	if (verbose)
		printf("Client sent transaction using Service Providers FD: %d\n",
		       obj->fd);
}

static void request_service_provider_fd(int fd, uint32_t handle)
{
	int result;
	uint32_t cmd;
	struct binder_write_read bwr;
	struct {
		uint32_t cmd;
		struct binder_transaction_data txn;
	} __attribute__((packed)) writebuf;
	unsigned int readbuf[32];

	memset(&writebuf, 0, sizeof(writebuf));
	memset(readbuf, 0, sizeof(readbuf));
	memset(&bwr, 0, sizeof(bwr));

	writebuf.cmd = BC_TRANSACTION;
	writebuf.txn.target.handle = handle;
	writebuf.txn.cookie = 0;
	writebuf.txn.code = TEST_SERVICE_SEND_FD;
	writebuf.txn.flags = TF_ACCEPT_FDS;

	writebuf.txn.data_size = 0;
	writebuf.txn.data.ptr.buffer = (binder_uintptr_t)NULL;
	writebuf.txn.data.ptr.offsets = (binder_uintptr_t)NULL;
	writebuf.txn.offsets_size = 0;

	bwr.write_size = sizeof(writebuf);
	bwr.write_consumed = 0;
	bwr.write_buffer = (binder_uintptr_t)&writebuf;
	bwr.read_size = sizeof(readbuf);
	bwr.read_consumed = 0;
	bwr.read_buffer = (binder_uintptr_t)readbuf;

	if (verbose)
		printf("Client sending transaction SEND_CLIENT_YOUR_BINDER_FD to handle: %d\n",
		       handle);

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr, "Client ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		exit(140);
	}

	cmd = binder_parse(fd, (binder_uintptr_t)readbuf, bwr.read_consumed);
	if (cmd == BR_FAILED_REPLY ||
	    cmd == BR_DEAD_REPLY ||
	    cmd == BR_DEAD_BINDER) {
		fprintf(stderr, "Client %s() failing command %s, exiting.\n",
			__func__, cmd_name(cmd));
		exit(141);
	}
}

static void extract_handle_and_acquire(int fd,
				       struct binder_transaction_data *txn_in)
{
	int result;
	uint32_t acmd[2];
	struct binder_write_read bwr;
	const struct flat_binder_object *obj;
	binder_size_t *offs = (binder_size_t *)
			      (binder_uintptr_t)txn_in->data.ptr.offsets;

	/* Get the fbo that contains the Service Provider's Handle. */
	obj = (const struct flat_binder_object *)
	      (((char *)(binder_uintptr_t)txn_in->data.ptr.buffer) + *offs);

	if (obj->hdr.type != BINDER_TYPE_HANDLE) {
		fprintf(stderr, "Client - Header not BINDER_TYPE_HANDLE: %d\n",
			obj->hdr.type);
		exit(150);
	}

	acmd[0] = BC_ACQUIRE;
	acmd[1] = obj->handle;

	memset(&bwr, 0, sizeof(bwr));
	bwr.write_buffer = (binder_uintptr_t)&acmd;
	bwr.write_size = sizeof(acmd);

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr,
			"Client ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		exit(151);
	}

	if (verbose)
		printf("Client acquired binder handle: %d for Service Provider\n",
		       obj->handle);

	/* Now get their fd */
	request_service_provider_fd(fd, obj->handle);

}

/* Parse response, reply as required and then return last CMD */
static int binder_parse(int fd, binder_uintptr_t ptr, binder_size_t size)
{
	binder_uintptr_t end = ptr + size;
	uint32_t cmd;

	while (ptr < end) {
		cmd = *(uint32_t *)ptr;
		ptr += sizeof(uint32_t);

		if (verbose)
			printf("Client command: %s\n", cmd_name(cmd));

		switch (cmd) {
		case BR_NOOP:
			break;
		case BR_TRANSACTION_COMPLETE:
			transactions_complete++;
			break;
		case BR_INCREFS:
		case BR_ACQUIRE:
		case BR_RELEASE:
		case BR_DECREFS:
			ptr += sizeof(const struct binder_ptr_cookie);
			break;
		case BR_TRANSACTION: {
			struct binder_transaction_data *txn =
				(struct binder_transaction_data *)ptr;

			if (verbose) {
				printf("Client BR_TRANSACTION data:\n");
				print_trans_data(txn);
			}

			ptr += sizeof(*txn);
			break;
		}
		case BR_REPLY: {
			struct binder_transaction_data *txn =
				(struct binder_transaction_data *)ptr;

			if (verbose) {
				printf("Client BR_REPLY data:\n");
				print_trans_data(txn);
			}

			if (txn->code == TEST_SERVICE_GET)
				extract_handle_and_acquire(fd, txn);

			if (txn->code == TEST_SERVICE_SEND_FD)
				extract_fd_and_respond(txn);

			ptr += sizeof(*txn);
			break;
		}
		case BR_DEAD_BINDER:
		case BR_FAILED_REPLY:
		case BR_DEAD_REPLY:
			break;
		case BR_ERROR:
			ptr += sizeof(uint32_t);
			break;
		default:
			if (verbose)
				printf("Client parsed unknown command: %d\n",
				       cmd);
			exit(160);
		}
	}
	fflush(stdout);
	return cmd;
}

int main(int argc, char **argv)
{
	int opt, result, fd, client_replies = 0, ioctl_wr = 0;
	bool name = false;
	bool use_transactions_complete = false;
	uint32_t cmd;
	pid_t pid;
	char *context;
	char dev_str[128];
	void *map_base;
	size_t map_size = BINDER_MMAP_SIZE;
	struct binder_write_read bwr;
	struct {
		uint32_t cmd;
		struct binder_transaction_data txn;
	} __attribute__((packed)) writebuf;
	unsigned int readbuf[32];

	transactions_complete = 0;
	fd_type = BINDER_FD;
	fd_type_str = "SP";

	while ((opt = getopt(argc, argv, "cnr:vmp")) != -1) {
		switch (opt) {
		case 'c':
			use_transactions_complete = true;
			break;
		case 'n':
			name = true;
			break;
		case 'r':
			client_replies = atoi(optarg);
			break;
		case 'v':
			verbose = true;
			break;
		case 'm':
			fd_type = BPF_MAP_FD;
			fd_type_str = "BPF map";
			break;
		case 'p':
			fd_type = BPF_PROG_FD;
			fd_type_str = "BPF prog";
			break;
		default:
			usage(argv[0]);
		}
	}

	/* Get our context and pid */
	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Client failed to obtain SELinux context\n");
		exit(120);
	}
	pid = getpid();

	if (verbose) {
		printf("Client PID: %d Process context:\n\t%s\n",
		       pid, context);
	}
	free(context);

	if (name)
		result = sprintf(dev_str, "%s/%s", BINDERFS_DEV, BINDERFS_NAME);
	else
		result = sprintf(dev_str, "%s", BINDER_DEV);

	if (result < 0) {
		fprintf(stderr, "Manager failed to obtain Binder dev name\n");
		exit(121);
	}

	fd = open(dev_str, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s error: %s\n", dev_str,
			strerror(errno));
		exit(122);
	}

	map_base = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_base == MAP_FAILED) {
		fprintf(stderr, "mmap error: %s\n", strerror(errno));
		close(fd);
		exit(123);
	}

	memset(&bwr, 0, sizeof(bwr));
	memset(&writebuf, 0, sizeof(writebuf));
	memset(readbuf, 0, sizeof(readbuf));

	writebuf.cmd = BC_TRANSACTION;
	writebuf.txn.target.handle = TEST_SERVICE_MANAGER_HANDLE;
	writebuf.txn.cookie = 0;
	writebuf.txn.code = TEST_SERVICE_GET;
	writebuf.txn.flags = TF_ACCEPT_FDS;

	writebuf.txn.data_size = 0;
	writebuf.txn.offsets_size = 0;

	writebuf.txn.data.ptr.buffer = (binder_uintptr_t)NULL;
	writebuf.txn.data.ptr.offsets = (binder_uintptr_t)NULL;

	bwr.write_buffer = (binder_uintptr_t)&writebuf;
	bwr.write_size = sizeof(writebuf);
	bwr.write_consumed = 0;

	if (verbose)
		printf("Client sending transaction to Manager - TEST_SERVICE_GET\n");

	/* Set an alarm just in case Binder does not reply when sitting
	 * on the ioctl BINDER_WRITE_READ
	 */
	signal(SIGALRM, client_alarm);
	alarm(10);

	/* Each Client test expects a different number of replies */
	while (true) {
		memset(readbuf, 0, sizeof(readbuf));
		bwr.read_size = sizeof(readbuf);
		bwr.read_consumed = 0;
		bwr.read_buffer = (binder_uintptr_t)readbuf;

		result = ioctl(fd, BINDER_WRITE_READ, &bwr);
		if (result < 0) {
			fprintf(stderr,
				"Client ioctl BINDER_WRITE_READ error: %s\n",
				strerror(errno));
			result = 124;
			goto brexit;
		}
		ioctl_wr++;

		if (verbose)
			printf("Client read_consumed: %lld\n",
			       bwr.read_consumed);

		cmd = binder_parse(fd, (binder_uintptr_t)readbuf,
				   bwr.read_consumed);

		if (cmd == BR_FAILED_REPLY ||
		    cmd == BR_DEAD_REPLY ||
		    cmd == BR_DEAD_BINDER) {
			fprintf(stderr, "Client %s() failing command %s, exiting.\n",
				__func__, cmd_name(cmd));
			result = 125;
			goto brexit;
		}

		result = 0;
		if (use_transactions_complete &&
		    transactions_complete == client_replies)
			break;
		else if (!use_transactions_complete &&
			 ioctl_wr == client_replies)
			break;
	}

brexit:
	munmap(map_base, map_size);
	close(fd);

	return result;
}

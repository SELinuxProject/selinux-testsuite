#include "binder_common.h"

/* Store the only service provider handle here. */
uint32_t sp_handle;

static int binder_parse(int fd, binder_uintptr_t ptr, binder_size_t size);
static void reply_with_handle(int fd, struct binder_transaction_data *txn_in);

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-f file] [-n] [-v]\n"
		"Where:\n\t"
		"-f  Write a line to the file when listening starts.\n\t"
		"-n  Use the /dev/binderfs name service.\n\t"
		"-v  Print context and command information.\n\t"
		"\nNote: Ensure this boolean command is run when "
		"testing after a reboot:\n\t"
		"setsebool allow_domain_fd_use=0\n", progname);
	exit(-1);
}

static void do_service_manager(int fd, struct binder_transaction_data *txn_in)
{
	int result;
	const struct flat_binder_object *obj;
	struct binder_write_read bwr;
	uint32_t acmd[2];
	binder_size_t *offs;

	struct {
		uint32_t cmd_reply;
		struct binder_transaction_data txn;
	} __attribute__((packed)) reply;

	switch (txn_in->code) {
	case TEST_SERVICE_ADD:
		offs = (binder_size_t *)(binder_uintptr_t)txn_in->data.ptr.offsets;

		/* Get fbo that contains the Service Providers handle. */
		obj = (const struct flat_binder_object *)
		      (((char *)(binder_uintptr_t)txn_in->data.ptr.buffer) + *offs);

		if (obj->hdr.type == BINDER_TYPE_HANDLE) {
			sp_handle = obj->handle;

			if (verbose)
				printf("Manager has TEST_SERVICE_ADD obj->handle: %d\n",
				       sp_handle);
		} else {
			fprintf(stderr, "Manager failed to obtain a handle\n");
			exit(20);
		}

		acmd[0] = BC_ACQUIRE;
		acmd[1] = obj->handle;

		memset(&bwr, 0, sizeof(bwr));
		bwr.write_buffer = (binder_uintptr_t)&acmd;
		bwr.write_size = sizeof(acmd);

		result = ioctl(fd, BINDER_WRITE_READ, &bwr);
		if (result < 0) {
			fprintf(stderr,
				"Manager ioctl BINDER_WRITE_READ error: %s\n",
				strerror(errno));
			exit(21);
		}

		if (verbose)
			printf("Manager acquired binder handle: %d for Service Provider\n",
			       sp_handle);

		/* Inform the Service Provider of outcome. */
		reply.cmd_reply = BC_REPLY;
		reply.txn.target.ptr = 0;
		reply.txn.cookie = txn_in->cookie;
		reply.txn.code = txn_in->code;
		reply.txn.flags = TF_STATUS_CODE;
		reply.txn.data_size = sizeof(int);
		reply.txn.offsets_size = 0;
		reply.txn.data.ptr.buffer = (binder_uintptr_t)&result;
		reply.txn.data.ptr.offsets = 0;
		binder_write(fd, &reply, sizeof(reply));
		break;
	case TEST_SERVICE_GET:
		if (verbose)
			printf("Manager has TEST_SERVICE_GET handle: %d\n",
			       sp_handle);

		reply_with_handle(fd, txn_in);

		break;
	case TEST_SERVICE_SEND_FD:
		if (verbose)
			printf("Manager Rx'ed SEND_CLIENT_YOUR_BINDER_FD for handle: %d\n",
			       txn_in->target.handle);
		break;
	default:
		fprintf(stderr, "Manager unknown txn->code: %d for handle: %d\n",
			txn_in->code, txn_in->target.handle);
		exit(22);
	}
}

static void reply_with_handle(int fd, struct binder_transaction_data *txn_in)
{
	int result;
	unsigned int writebuf[1024];
	binder_size_t offset = 0;
	struct flat_binder_object obj;
	struct binder_write_read bwr;
	struct binder_transaction_data *txn;

	if (verbose)
		printf("Manager sending BC_REPLY to Client with handle\n");

	memset(&writebuf, 0, sizeof(writebuf));
	memset(&obj, 0, sizeof(obj));
	memset(&bwr, 0, sizeof(bwr));

	writebuf[0] = BC_REPLY;
	txn = (struct binder_transaction_data *)(&writebuf[1]);
	memset(txn, 0, sizeof(*txn));
	txn->target.handle = txn_in->target.handle;
	txn->cookie = txn_in->cookie;
	txn->code = txn_in->code;
	txn->flags = TF_ACCEPT_FDS;

	obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
	obj.hdr.type = BINDER_TYPE_HANDLE;
	obj.handle = sp_handle;
	obj.cookie = 0;

	txn->data_size = sizeof(obj);
	txn->data.ptr.buffer = (binder_uintptr_t)&obj;
	txn->data.ptr.offsets = (binder_uintptr_t)&offset;
	txn->offsets_size = sizeof(offset);

	bwr.write_buffer = (binder_uintptr_t)writebuf;
	bwr.write_size = sizeof(writebuf[0]) + sizeof(*txn);

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr, "Manager ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		exit(30);
	}

	if (verbose)
		printf("Manager sent Service Provider handle: %d to Client\n",
		       sp_handle);
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
			printf("Manager command: %s\n", cmd_name(cmd));

		switch (cmd) {
		case BR_NOOP:
			break;
		case BR_TRANSACTION_COMPLETE:
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
				printf("Manager BR_TRANSACTION data:\n");
				print_trans_data(txn);
			}

			do_service_manager(fd, txn);
			ptr += sizeof(*txn);
			break;
		}
		case BR_REPLY: {
			struct binder_transaction_data *txn =
				(struct binder_transaction_data *)ptr;

			if (verbose) {
				printf("Manager BR_REPLY data:\n");
				print_trans_data(txn);
			}

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
				printf("Manager parsed unknown command: %d\n",
				       cmd);
			exit(40);
		}
	}
	fflush(stdout);
	return cmd;
}

int main(int argc, char **argv)
{
	int opt, result, fd;
	bool name = false;
	pid_t pid;
	char *context;
	char *flag_file = NULL;
	char dev_str[128];
	FILE *flag_fd;
	void *map_base;
	size_t map_size = BINDER_MMAP_SIZE;
	struct binder_write_read bwr;
	unsigned int readbuf[32];

	while ((opt = getopt(argc, argv, "f:nv")) != -1) {
		switch (opt) {
		case 'f':
			flag_file = optarg;
			break;
		case 'n':
			name = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	/* Get our context and pid */
	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Manager failed to obtain SELinux context\n");
		exit(10);
	}
	pid = getpid();

	if (verbose) {
		printf("Manager PID: %d Process context:\n\t%s\n",
		       pid, context);
	}
	free(context);

	if (name)
		result = sprintf(dev_str, "%s/%s", BINDERFS_DEV, BINDERFS_NAME);
	else
		result = sprintf(dev_str, "%s", BINDER_DEV);

	if (result < 0) {
		fprintf(stderr, "Manager failed to obtain Binder dev name\n");
		exit(11);
	}

	fd = open(dev_str, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Manager cannot open %s error: %s\n",
			dev_str, strerror(errno));
		exit(12);
	}

	map_base = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_base == MAP_FAILED) {
		fprintf(stderr, "Manager mmap error: %s\n", strerror(errno));
		close(fd);
		exit(13);
	}

	result = ioctl(fd, BINDER_SET_CONTEXT_MGR, 0);
	if (result < 0) {
		fprintf(stderr,
			"Manager failed to become context manager: %s\n",
			strerror(errno));
		result = 14;
		goto brexit;
	}

	if (flag_file) {
		flag_fd = fopen(flag_file, "w");
		if (!flag_fd) {
			fprintf(stderr,
				"Manager failed to open %s: %s\n",
				flag_file, strerror(errno));
			result = 15;
			goto brexit;
		}
		fprintf(flag_fd, "listening\n");
		fclose(flag_fd);
	}

	memset(readbuf, 0, sizeof(readbuf));
	memset(&bwr, 0, sizeof(bwr));

	readbuf[0] = BC_ENTER_LOOPER;
	bwr.write_size = sizeof(readbuf[0]);
	bwr.write_consumed = 0;
	bwr.write_buffer = (binder_uintptr_t)readbuf;
	bwr.read_size = 0;
	bwr.read_consumed = 0;
	bwr.read_buffer = 0;

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr,
			"Manager ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		result = 16;
		goto brexit;
	}

	while (true) {
		memset(readbuf, 0, sizeof(readbuf));
		bwr.read_size = sizeof(readbuf);
		bwr.read_consumed = 0;
		bwr.read_buffer = (binder_uintptr_t)readbuf;

		result = ioctl(fd, BINDER_WRITE_READ, &bwr);
		if (result < 0) {
			fprintf(stderr,
				"Manager ioctl BINDER_WRITE_READ error: %s\n",
				strerror(errno));
			result = 17;
			goto brexit;
		}

		if (bwr.read_consumed == 0)
			continue;

		if (verbose)
			printf("Manager read_consumed: %lld\n",
			       bwr.read_consumed);

		binder_parse(fd, (binder_uintptr_t)readbuf, bwr.read_consumed);
	}

brexit:
	munmap(map_base, map_size);
	close(fd);

	return result;
}

#include "binder_common.h"

static char *expected_type;
static int binder_parse(int fd, binder_uintptr_t ptr, binder_size_t size);

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-e expected_type] [-f file] [-n] [-m|-p|-t] [-v]\n"
		"Where:\n\t"
		"-e  Expected security type.\n\t"
		"-f  Write a line to the file when listening starts.\n\t"
		"-n  Use the /dev/binderfs name service.\n\t"
		"-m  Use BPF map fd for transfer.\n\t"
		"-p  Use BPF prog fd for transfer.\n\t"
		"-t  Test if BPF enabled.\n\t"
		"-v  Print context and command information.\n\t"
		"\nNote: Ensure this boolean command is run when "
		"testing after a reboot:\n\t"
		"setsebool allow_domain_fd_use=0\n", progname);
	exit(-1);
}

static void request_service_provider_fd(int fd,
					struct binder_transaction_data *txn_in)
{
	int result;
	unsigned int writebuf[1024];
	struct binder_fd_object obj;
	binder_size_t offset = 0;
	struct binder_write_read bwr;
	struct binder_transaction_data *txn;

	if (txn_in->flags == TF_ONE_WAY && verbose) {
		printf("Service Provider no reply to BC_TRANSACTION as flags = TF_ONE_WAY\n");
		return;
	}

	if (verbose)
		printf("Service Provider sending BC_REPLY with an FD\n");

	memset(writebuf, 0, sizeof(writebuf));
	memset(&bwr, 0, sizeof(bwr));
	memset(&obj, 0, sizeof(obj));

	writebuf[0] = BC_REPLY;
	txn = (struct binder_transaction_data *)(&writebuf[1]);
	memset(txn, 0, sizeof(*txn));
	txn->target.handle = txn_in->target.handle;
	txn->cookie = txn_in->cookie;
	txn->code = txn_in->code;
	txn->flags = TF_ACCEPT_FDS;

	obj.hdr.type = BINDER_TYPE_FD;
#if HAVE_BINDERFS
	obj.pad_flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS |
			FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
#else
	obj.pad_flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
#endif
	obj.cookie = txn->cookie;

	/*
	 * The Service Providers binder fd is used for testing as it allows
	 * policy to set whether the Service Provider and Client can be
	 * allowed access (fd use) or not.
	 * This also allows a check for the impersonate permission later as
	 * the Client will use the Service Provider fd to send a transaction.
	 *
	 * If a BPF fd is required, it is generated, however it cannot be
	 * used to check the impersonate permission.
	 */
	switch (fd_type) {
	case BINDER_FD:
		obj.fd = fd;
		break;
#if HAVE_BPF
	case BPF_MAP_FD:
		result = create_bpf_map();
		if (result < 0)
			exit(70);
		obj.fd = result;
		break;
	case BPF_PROG_FD:
		result = create_bpf_prog();
		if (result < 0)
			exit(71);
		obj.fd = result;
		break;
#else
	case BPF_MAP_FD:
	case BPF_PROG_FD:
		fprintf(stderr, "BPF not supported - Service Provider\n");
		exit(72);
		break;
#endif
	default:
		fprintf(stderr, "Invalid fd_type: %d\n", fd_type);
		exit(73);
	}

	if (verbose)
		printf("Service Provider handle: %d and %s FD: %d\n",
		       txn->target.handle, fd_type_str, obj.fd);

	txn->data_size = sizeof(obj);
	txn->data.ptr.buffer = (binder_uintptr_t)&obj;
	txn->data.ptr.offsets = (binder_uintptr_t)&offset;
	txn->offsets_size = sizeof(offset);

	bwr.write_buffer = (binder_uintptr_t)writebuf;
	bwr.write_size = sizeof(writebuf[0]) + sizeof(*txn);

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr,
			"Service Provider ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		exit(74);
	}
}

/* Parse response, reply as required and then return last CMD */
static int binder_parse(int fd, binder_uintptr_t ptr, binder_size_t size)
{
	binder_uintptr_t end = ptr + size;
	uint32_t cmd = BR_DEAD_REPLY;

	while (ptr < end) {
		cmd = *(uint32_t *)ptr;
		ptr += sizeof(uint32_t);

		if (verbose)
			printf("Service Provider command: %s\n",
			       cmd_name(cmd));

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
				printf("Service Provider BR_TRANSACTION data:\n");
				print_trans_data(txn);
			}

			if (txn->code == TEST_SERVICE_SEND_FD)
				request_service_provider_fd(fd, txn);

			ptr += sizeof(*txn);
			break;
		}
#if HAVE_BINDERFS
		case BR_TRANSACTION_SEC_CTX: {
			struct binder_transaction_data_secctx *txn_ctx =
				(struct binder_transaction_data_secctx *)ptr;
			if (verbose) {
				printf("\tclient context:\n\t\t%s\n",
				       (char *)txn_ctx->secctx);
				print_trans_data(&txn_ctx->transaction_data);
			}

			if (expected_type) {
				context_t ctx = context_new((const char *)txn_ctx->secctx);

				if (!ctx) {
					fprintf(stderr,
						"Service Provider context_new: %s\n",
						strerror(errno));
					exit(82);
				}

				if (strcmp(expected_type, context_type_get(ctx))) {
					fprintf(stderr, "Service Provider received incorrect context:\n");
					fprintf(stderr, "Expected: %s\nReceived: %s\n",
						expected_type,
						context_type_get(ctx));
					exit(80);
				}
				context_free(ctx);
			}

			if (txn_ctx->transaction_data.code == TEST_SERVICE_SEND_FD)
				request_service_provider_fd(fd,
							    &txn_ctx->transaction_data);

			ptr += sizeof(*txn_ctx);
			break;
		}
#endif
		case BR_REPLY: {
			struct binder_transaction_data *txn =
				(struct binder_transaction_data *)ptr;

			if (verbose) {
				printf("Service Provider BR_REPLY data:\n");
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
				printf("Service Provider parsed unknown command: %d\n",
				       cmd);
			exit(81);
		}
	}
	fflush(stdout);
	return cmd;
}

int main(int argc, char **argv)
{
	int opt, result, fd;
	bool name = false;
	uint32_t cmd;
	pid_t pid;
	char *context;
	char *flag_file = NULL;
	char dev_str[128];
	FILE *flag_fd;
	void *map_base;
	size_t map_size = BINDER_MMAP_SIZE;
	binder_size_t offset = 0;
	struct binder_write_read bwr;
	struct flat_binder_object obj;
	struct {
		uint32_t cmd;
		struct binder_transaction_data txn;
	} __attribute__((packed)) writebuf;
	unsigned int readbuf[32];

	expected_type = NULL;
	fd_type = BINDER_FD;
	fd_type_str = "SP";

	while ((opt = getopt(argc, argv, "e:f:nvmpt")) != -1) {
		switch (opt) {
		case 'e':
			expected_type = optarg;
			break;
		case 'f':
			flag_file = optarg;
			break;
		case 'n':
			name = true;
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
		case 't':
			fd_type = BPF_TEST;
			break;
		default:
			usage(argv[0]);
		}
	}


#if HAVE_BPF
	if (fd_type == BPF_TEST)
		exit(0);

	/* If BPF enabled, then need to set limits */
	if (fd_type == BPF_MAP_FD || fd_type == BPF_PROG_FD)
		bpf_setrlimit();
#else
	if (fd_type == BPF_TEST) {
		fprintf(stderr, "BPF not supported\n");
		exit(-1);
	}
#endif

	/* Get our context and pid */
	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Service Provider failed to obtain SELinux context\n");
		exit(60);
	}
	pid = getpid();

	if (verbose) {
		printf("Service Provider PID: %d Process context:\n\t%s\n",
		       pid, context);
	}
	free(context);

	if (name)
		result = sprintf(dev_str, "%s/%s", BINDERFS_DEV, BINDERFS_NAME);
	else
		result = sprintf(dev_str, "%s", BINDER_DEV);

	if (result < 0) {
		fprintf(stderr, "Manager failed to obtain Binder dev name\n");
		exit(61);
	}

	fd = open(dev_str, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Cannot open %s error: %s\n", dev_str,
			strerror(errno));
		exit(62);
	}

	map_base = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (map_base == MAP_FAILED) {
		fprintf(stderr, "mmap error: %s\n", strerror(errno));
		close(fd);
		exit(63);
	}

	memset(&writebuf, 0, sizeof(writebuf));
	memset(&obj, 0, sizeof(obj));
	memset(readbuf, 0, sizeof(readbuf));

	writebuf.cmd = BC_TRANSACTION;
	writebuf.txn.target.handle = TEST_SERVICE_MANAGER_HANDLE;
	writebuf.txn.cookie = 0;
	writebuf.txn.code = TEST_SERVICE_ADD;
	writebuf.txn.flags = TF_ROOT_OBJECT;

	obj.hdr.type = BINDER_TYPE_BINDER;
#if HAVE_BINDERFS
	obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS |
		    FLAT_BINDER_FLAG_TXN_SECURITY_CTX;
#else
	obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
#endif
	obj.binder = (binder_uintptr_t)NULL;
	obj.cookie = 0;

	writebuf.txn.data_size = sizeof(obj);
	writebuf.txn.offsets_size = sizeof(offset);

	writebuf.txn.data.ptr.buffer = (binder_uintptr_t)&obj;
	writebuf.txn.data.ptr.offsets = (binder_uintptr_t)&offset;

	bwr.write_buffer = (binder_uintptr_t)&writebuf;
	bwr.write_size = sizeof(writebuf);
	bwr.write_consumed = 0;

	if (verbose)
		printf("Service Provider sending transaction to Manager - TEST_SERVICE_ADD\n");

	bwr.read_size = sizeof(readbuf);
	bwr.read_consumed = 0;
	bwr.read_buffer = (binder_uintptr_t)readbuf;

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr,
			"Service Provider ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		result = 64;
		goto brexit;
	}

	if (verbose)
		printf("Service Provider read_consumed: %lld\n",
		       bwr.read_consumed);

	cmd = binder_parse(fd, (binder_uintptr_t)readbuf,
			   bwr.read_consumed);

	if (cmd == BR_FAILED_REPLY ||
	    cmd == BR_DEAD_REPLY ||
	    cmd == BR_DEAD_BINDER) {
		fprintf(stderr, "Service Provider %s() failing command %s, exiting.\n",
			__func__, cmd_name(cmd));
		result = 65;
		goto brexit;
	}

	/* Service Provider loops on commands */
	memset(readbuf, 0, sizeof(readbuf));
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
			"Service Provider ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
		result = 66;
		goto brexit;
	}

	/*
	 * Ensure the Manager and Service Provider have completed the
	 * TEST_SERVICE_ADD sequence before the Client is allowed to start.
	 */
	if (flag_file) {
		flag_fd = fopen(flag_file, "w");
		if (!flag_fd) {
			fprintf(stderr,
				"Service Provider failed to open %s: %s\n",
				flag_file, strerror(errno));
			result = 67;
			goto brexit;
		}
		fprintf(flag_fd, "listening\n");
		fclose(flag_fd);
	}

	while (true) {
		memset(readbuf, 0, sizeof(readbuf));
		bwr.read_size = sizeof(readbuf);
		bwr.read_consumed = 0;
		bwr.read_buffer = (binder_uintptr_t)readbuf;

		result = ioctl(fd, BINDER_WRITE_READ, &bwr);
		if (result < 0) {
			fprintf(stderr,
				"Service Provider ioctl BINDER_WRITE_READ error: %s\n",
				strerror(errno));
			result = 68;
			goto brexit;
		}

		if (bwr.read_consumed == 0)
			continue;

		if (verbose)
			printf("Service Provider read_consumed: %lld\n",
			       bwr.read_consumed);

		cmd = binder_parse(fd, (binder_uintptr_t)readbuf, bwr.read_consumed);

		if (cmd == BR_FAILED_REPLY ||
		    cmd == BR_DEAD_REPLY ||
		    cmd == BR_DEAD_BINDER) {
			fprintf(stderr, "Service Provider %s() failing command %s, exiting.\n",
				__func__, cmd_name(cmd));
			result = 69;
			goto brexit;
		}
	}

brexit:
	munmap(map_base, map_size);
	close(fd);

	return result;
}

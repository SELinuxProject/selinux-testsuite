/*
 * This is a simple binder Service Manager/Service Provider that only uses
 * the raw ioctl commands to test the SELinux binder permissions:
 *     set_context_mgr, call, transfer, impersonate.
 *
 * Using binder test policy the following will be validated:
 *    security_binder_set_context_mgr() binder { set_context_mgr }
 *    security_binder_transaction()     binder { call impersonate }
 *    security_binder_transfer_binder() binder { transfer }
 *    security_binder_transfer_file()   fd { use }
 *
 * TODO security_binder_transfer_file() uses BPF if configured in kernel.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <selinux/selinux.h>
#include <linux/android/binder.h>

/* These are the Binder txn->code values used by the Service Provider and
 * Manager to request/retrieve a binder handle.
 */
#define ADD_TEST_SERVICE 100 /* Sent by Service Provider */
#define GET_TEST_SERVICE 101 /* Sent by Client */

#define TEST_SERVICE_MANAGER_HANDLE 0

static int binder_parse(int fd, uintptr_t ptr, size_t size, bool manager);

static bool verbose;
uint32_t sp_handle;
static unsigned char *shm_base;
static int shm_fd;
static int shm_size = 32;

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-r replies] [-v] manager | provider\n"
		"Where:\n\t"
		"-r       Number of replies to expect from test - default 0.\n\t"
		"-v       Print context and command information.\n\t"
		"manager  Act as Service Manager.\n\t"
		"service  Act as Service Provider.\n"
		"\nNote: Ensure this boolean command is run when "
		"testing after a reboot:\n\t"
		"setsebool allow_domain_fd_use=0\n", progname);
	exit(-1);
}

/* Create a small piece of shared memory between the Manager and Service
 * Provider to share a handle as explained in the do_service_manager()
 * function.
 */
static void create_shm(bool manager)
{
	char *name =  "sp_handle";

	if (manager)
		shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);
	else
		shm_fd = shm_open(name, O_RDONLY, 0666);
	if (shm_fd < 0) {
		fprintf(stderr, "%s shm_open: %s\n", __func__, strerror(errno));
		exit(-1);
	}

	ftruncate(shm_fd, shm_size);

	if (manager)
		shm_base = mmap(0, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED,
				shm_fd, 0);
	else
		shm_base = mmap(0, shm_size, PROT_READ, MAP_SHARED, shm_fd, 0);
	if (shm_base == MAP_FAILED) {
		fprintf(stderr, "%s mmap: %s\n", __func__, strerror(errno));
		close(shm_fd);
		exit(-1);
	}
}

static const char *cmd_name(uint32_t cmd)
{
	switch (cmd) {
	case BR_NOOP:
		return "BR_NOOP";
	case BR_TRANSACTION_COMPLETE:
		return "BR_TRANSACTION_COMPLETE";
	case BR_INCREFS:
		return "BR_INCREFS";
	case BR_ACQUIRE:
		return "BR_ACQUIRE";
	case BR_RELEASE:
		return "BR_RELEASE";
	case BR_DECREFS:
		return "BR_DECREFS";
	case BR_TRANSACTION:
		return "BR_TRANSACTION";
	case BR_REPLY:
		return "BR_REPLY";
	case BR_FAILED_REPLY:
		return "BR_FAILED_REPLY";
	case BR_DEAD_REPLY:
		return "BR_DEAD_REPLY";
	case BR_DEAD_BINDER:
		return "BR_DEAD_BINDER";
	case BR_ERROR:
		return "BR_ERROR";
	/* fallthrough */
	default:
		return "Unknown command";
	}
}

static void print_trans_data(struct binder_transaction_data *txn_in)
{
	struct flat_binder_object *obj;
	binder_size_t *offs = (binder_size_t *)
			      (uintptr_t)txn_in->data.ptr.offsets;
	size_t count = txn_in->offsets_size / sizeof(binder_size_t);

	printf("\thandle: %ld\n", (unsigned long)txn_in->target.handle);
	printf("\tcookie: %lld\n", txn_in->cookie);
	printf("\tcode: %d\n", txn_in->code);
	switch (txn_in->flags) {
	case TF_ONE_WAY:
		printf("\tflag: TF_ONE_WAY\n");
		break;
	case TF_ROOT_OBJECT:
		printf("\tflag: TF_ROOT_OBJECT\n");
		break;
	case TF_STATUS_CODE:
		printf("\tflag: TF_STATUS_CODE\n");
		break;
	case TF_ACCEPT_FDS:
		printf("\tflag: TF_ACCEPT_FDS\n");
		break;
	default:
		printf("Unknown flag: %x\n", txn_in->flags);
		return;
	}
	printf("\tsender pid: %u\n", txn_in->sender_pid);
	printf("\tsender euid: %u\n", txn_in->sender_euid);
	printf("\tdata_size: %llu\n", txn_in->data_size);
	printf("\toffsets_size: %llu\n", txn_in->offsets_size);

	while (count--) {
		obj = (struct flat_binder_object *)
		      (((char *)(uintptr_t)txn_in->data.ptr.buffer) + *offs++);

		switch (obj->hdr.type) {
		case BINDER_TYPE_BINDER:
			printf("\thdr: BINDER_TYPE_BINDER\n");
			printf("\tbinder: %llx\n", obj->binder);
			break;
		case BINDER_TYPE_HANDLE:
			printf("\thdr: BINDER_TYPE_HANDLE\n");
			printf("\thandle: %x\n", obj->handle);
			break;
		case BINDER_TYPE_FD:
			printf("\thdr: BINDER_TYPE_FD\n");
			printf("\tfd: %x\n", obj->handle);
			break;
		default:
			printf("Unknown header: %u\n", obj->hdr.type);
			return;
		}
		printf("\tflags: priority: 0x%x accept FDS: %s\n",
		       obj->flags & FLAT_BINDER_FLAG_PRIORITY_MASK,
		       obj->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS ? "YES" : "NO");
		printf("\tcookie: %llx\n", obj->cookie);
	}
}

/* If add a service provider, then obtain a handle for it and store in
 * shared memory. The handle will then be used by the service provider
 * process to contact the Manager for its file descriptor, thus triggering
 * the 'impersonate' permission (as current_sid() != task_sid(from))
 * It is done this way as being a cheapskate it saved adding code to the
 * GET_TEST_SERVICE process plus running a Client as well. This achieves
 * the same objective.
 */
static void do_service_manager(int fd, struct binder_transaction_data *txn_in)
{
	int result;
	struct flat_binder_object *obj;
	struct binder_write_read bwr;
	uint32_t acmd[2];
	binder_size_t *offs;

	switch (txn_in->code) {
	case ADD_TEST_SERVICE:
		offs = (binder_size_t *)(uintptr_t)txn_in->data.ptr.offsets;

		/* Get fbo that contains the Managers binder file descriptor. */
		obj = (struct flat_binder_object *)
		      (((char *)(uintptr_t)txn_in->data.ptr.buffer) + *offs);

		if (obj->hdr.type == BINDER_TYPE_HANDLE) {
			sp_handle = obj->handle;
			memcpy(shm_base, &sp_handle, sizeof(sp_handle));
			if (verbose)
				printf("Manager has BINDER_TYPE_HANDLE obj->handle: %d\n",
				       sp_handle);
		} else {
			fprintf(stderr, "Failed to obtain a handle\n");
			exit(-1);
		}

		acmd[0] = BC_ACQUIRE;
		acmd[1] = obj->handle;

		memset(&bwr, 0, sizeof(bwr));
		bwr.write_buffer = (uintptr_t)&acmd;
		bwr.write_size = sizeof(acmd);

		result = ioctl(fd, BINDER_WRITE_READ, &bwr);
		if (result < 0) {
			fprintf(stderr,
				"ServiceProvider ioctl BINDER_WRITE_READ: %s\n",
				strerror(errno));
			exit(-1);
		}

		if (verbose)
			printf("Manager acquired handle: %d for Service Provider\n",
			       sp_handle);
		break;

	case GET_TEST_SERVICE:
		if (verbose)
			printf("GET_TEST_SERVICE not supported\n");
		break;
	default:
		fprintf(stderr, "Unknown txn->code: %d\n", txn_in->code);
		exit(-1);
	}
}

static void request_manager_fd(int fd, struct binder_transaction_data *txn_in)
{
	int result;
	unsigned int writebuf[1024];
	struct binder_fd_object obj;
	struct binder_write_read bwr;
	struct binder_transaction_data *txn;

	if (txn_in->flags == TF_ONE_WAY) {
		if (verbose)
			printf("Manager no reply to BC_TRANSACTION as flags = TF_ONE_WAY\n");
		return;
	}

	if (verbose)
		printf("Manager sending BC_REPLY to obtain its FD\n");

	writebuf[0] = BC_REPLY;
	txn = (struct binder_transaction_data *)(&writebuf[1]);

	memset(txn, 0, sizeof(*txn));
	txn->target.handle = txn_in->target.handle;
	txn->cookie = txn_in->cookie;
	txn->code = txn_in->code;
	txn->flags = TF_ACCEPT_FDS;
	memset(&obj, 0, sizeof(struct binder_fd_object));
	obj.hdr.type = BINDER_TYPE_FD;
	obj.pad_flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
	obj.cookie = txn->cookie;
	/* The binder fd is used for testing as it allows policy to set
	 * whether the Service and Manager can be allowed access (fd use)
	 * or not. For example test_binder_mgr_t has:
	 *        allow test_binder_service_t test_binder_mgr_t:fd use;
	 * whereas test_binder_mgr_no_fd_t does not allow this fd use.
	 *
	 * This also allows a check for the impersonate permission later
	 * as the Service Provider will use this (the Managers fd) to
	 * send a transaction.
	 */
	obj.fd = fd;

	if (verbose)
		printf("Manager handle: %d and its FD: %d\n",
		       txn->target.handle, fd);

	txn->data_size = sizeof(struct flat_binder_object);
	txn->data.ptr.buffer = (uintptr_t)&obj;
	txn->data.ptr.offsets = (uintptr_t)&obj +
				sizeof(struct flat_binder_object);
	txn->offsets_size = sizeof(binder_size_t);

	memset(&bwr, 0, sizeof(bwr));
	bwr.write_buffer = (uintptr_t)writebuf;
	bwr.write_size = sizeof(writebuf[0]) + sizeof(*txn);

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr, "%s ioctl BINDER_WRITE_READ: %s\n",
			__func__, strerror(errno));
		exit(-1);
	}
}

/* This retrieves the requested Managers file descriptor, then using this
 * sends a simple transaction to trigger the impersonate permission.
 */
static void extract_fd_and_respond(struct binder_transaction_data *txn_in,
				   bool manager)
{
	int result;
	uint32_t cmd;
	struct stat sb;
	struct binder_write_read bwr;
	struct binder_fd_object *obj;
	struct {
		uint32_t cmd;
		struct binder_transaction_data txn;
	} __attribute__((packed)) writebuf;
	unsigned int readbuf[32];

	binder_size_t *offs = (binder_size_t *)
			      (uintptr_t)txn_in->data.ptr.offsets;

	/* Get the bfdo that contains the Managers binder file descriptor. */
	obj = (struct binder_fd_object *)
	      (((char *)(uintptr_t)txn_in->data.ptr.buffer) + *offs);

	if (obj->hdr.type != BINDER_TYPE_FD) {
		fprintf(stderr, "Header not BINDER_TYPE_FD: %d\n",
			obj->hdr.type);
		exit(100);
	}

	/* fstat this just to see if a valid fd */
	result = fstat(obj->fd, &sb);
	if (result < 0) {
		fprintf(stderr, "Not a valid fd: %s\n", strerror(errno));
		exit(101);
	}

	if (verbose)
		printf("Service Provider retrieved Managers fd: %d st_dev: %ld\n",
		       obj->fd, sb.st_dev);

	/* Send response using Managers fd to trigger impersonate check. */
	writebuf.cmd = BC_TRANSACTION;
	memcpy(&writebuf.txn.target.handle, shm_base, sizeof(uint32_t));

	writebuf.txn.cookie = 0;
	writebuf.txn.code = 0;
	writebuf.txn.flags = TF_ONE_WAY;

	writebuf.txn.data_size = 0;
	writebuf.txn.data.ptr.buffer = (uintptr_t)NULL;
	writebuf.txn.data.ptr.offsets = (uintptr_t)NULL;
	writebuf.txn.offsets_size = 0;

	memset(&bwr, 0, sizeof(bwr));
	bwr.write_size = sizeof(writebuf);
	bwr.write_consumed = 0;
	bwr.write_buffer = (uintptr_t)&writebuf;
	bwr.read_size = sizeof(readbuf);
	bwr.read_consumed = 0;
	bwr.read_buffer = (uintptr_t)readbuf;

	result = ioctl(obj->fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr,
			"Service Provider ioctl BINDER_WRITE_READ: %s\n",
			strerror(errno));
		exit(102);
	}

	if (verbose)
		printf("Service Provider read_consumed: %lld\n",
		       bwr.read_consumed);

	cmd = binder_parse(obj->fd, (uintptr_t)readbuf,
			   bwr.read_consumed, manager);

	if (verbose)
		printf("Service Provider using Managers FD\n");

	if (cmd == BR_FAILED_REPLY ||
	    cmd == BR_DEAD_REPLY ||
	    cmd == BR_DEAD_BINDER) {
		fprintf(stderr,
			"Failed response from Service Provider using Managers FD\n");
		exit(103);
	}
}

/* Parse response, reply as required and then return last CMD */
static int binder_parse(int fd, uintptr_t ptr, size_t size, bool manager)
{
	uintptr_t end = ptr + (uintptr_t)size;
	uint32_t cmd;

	while (ptr < end) {
		cmd = *(uint32_t *)ptr;
		ptr += sizeof(uint32_t);

		if (verbose)
			printf("%s command: %s\n",
			       manager ? "Manager" : "Service Provider",
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
			ptr += sizeof(struct binder_ptr_cookie);
			break;
		case BR_TRANSACTION: {
			struct binder_transaction_data *txn =
				(struct binder_transaction_data *)ptr;

			if (verbose) {
				printf("%s BR_TRANSACTION data:\n",
				       manager ? "Manager" : "Service Provider");
				print_trans_data(txn);
			}

			if (manager) {
				do_service_manager(fd, txn);
				request_manager_fd(fd, txn);
			}

			ptr += sizeof(*txn);
			break;
		}
		case BR_REPLY: {
			struct binder_transaction_data *txn =
				(struct binder_transaction_data *)ptr;

			if (verbose) {
				printf("%s BR_REPLY data:\n",
				       manager ? "Manager" : "Service Provider");
				print_trans_data(txn);
			}

			/* Service Provider extracts the Manager fd, and responds */
			if (!manager)
				extract_fd_and_respond(txn, manager);

			ptr += sizeof(*txn);
			break;
		}
		case BR_DEAD_BINDER:
			break;
		case BR_FAILED_REPLY:
			break;
		case BR_DEAD_REPLY:
			break;
		case BR_ERROR:
			ptr += sizeof(uint32_t);
			break;
		default:
			if (verbose)
				printf("%s Parsed unknown command: %d\n",
				       manager ? "Manager" : "Service Provider",
				       cmd);
			exit(-1);
		}
	}

	return cmd;
}

int main(int argc, char **argv)
{
	int opt, result, binder_fd, provider_replies = 0;
	uint32_t cmd;
	bool manager;
	pid_t pid;
	char *driver = "/dev/binder";
	char *context;
	void *map_base;
	size_t map_size = 1024 * 8;
	struct binder_write_read bwr;
	struct flat_binder_object obj;
	struct {
		uint32_t cmd;
		struct binder_transaction_data txn;
	} __attribute__((packed)) writebuf;
	unsigned int readbuf[32];

	verbose = false;

	while ((opt = getopt(argc, argv, "vr:")) != -1) {
		switch (opt) {
		case 'v':
			verbose = true;
			break;
		case 'r':
			provider_replies = atoi(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 1)
		usage(argv[0]);

	if (!strcmp(argv[optind], "manager"))
		manager = true;
	else if (!strcmp(argv[optind], "provider"))
		manager = false;
	else
		usage(argv[0]);

	binder_fd = open(driver, O_RDWR | O_CLOEXEC);
	if (binder_fd < 0) {
		fprintf(stderr, "Cannot open %s error: %s\n", driver,
			strerror(errno));
		exit(1);
	}

	map_base = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, binder_fd, 0);
	if (map_base == MAP_FAILED) {
		fprintf(stderr, "mmap error: %s\n", strerror(errno));
		close(binder_fd);
		exit(2);
	}

	/* Create the appropriate shared memory for passing the Service
	 * Providers handle from the Manager to the Service Provider for
	 * use in the impersonate tests. This saves adding a Client to
	 * do this job.
	 */
	create_shm(manager);

	/* Get our context and pid */
	result = getcon(&context);
	if (result < 0) {
		fprintf(stderr, "Failed to obtain SELinux context\n");
		result = 3;
		goto brexit;
	}
	pid = getpid();

	if (manager) { /* Service Manager */
		if (verbose) {
			printf("Manager PID: %d Process context:\n\t%s\n",
			       pid, context);
		}

		result = ioctl(binder_fd, BINDER_SET_CONTEXT_MGR, 0);
		if (result < 0) {
			fprintf(stderr,
				"Failed to become context manager: %s\n",
				strerror(errno));
			result = 4;
			goto brexit;
		}

		readbuf[0] = BC_ENTER_LOOPER;
		bwr.write_size = sizeof(readbuf[0]);
		bwr.write_consumed = 0;
		bwr.write_buffer = (uintptr_t)readbuf;

		bwr.read_size = 0;
		bwr.read_consumed = 0;
		bwr.read_buffer = 0;

		result = ioctl(binder_fd, BINDER_WRITE_READ, &bwr);
		if (result < 0) {
			fprintf(stderr,
				"Manager ioctl BINDER_WRITE_READ: %s\n",
				strerror(errno));
			result = 5;
			goto brexit;
		}

		while (true) {
			bwr.read_size = sizeof(readbuf);
			bwr.read_consumed = 0;
			bwr.read_buffer = (uintptr_t)readbuf;

			result = ioctl(binder_fd, BINDER_WRITE_READ, &bwr);
			if (result < 0) {
				fprintf(stderr,
					"Manager ioctl BINDER_WRITE_READ: %s\n",
					strerror(errno));
				result = 6;
				goto brexit;
			}

			if (bwr.read_consumed == 0)
				continue;

			if (verbose)
				printf("Manager read_consumed: %lld\n",
				       bwr.read_consumed);

			cmd = binder_parse(binder_fd, (uintptr_t)readbuf,
					   bwr.read_consumed, manager);
		}
	} else { /* Service Provider */
		if (verbose) {
			printf("Service Provider PID: %d Process context:\n\t%s\n",
			       pid, context);
		}

		writebuf.cmd = BC_TRANSACTION;
		writebuf.txn.target.handle = TEST_SERVICE_MANAGER_HANDLE;
		writebuf.txn.cookie = 0;
		writebuf.txn.code = ADD_TEST_SERVICE;
		writebuf.txn.flags = TF_ACCEPT_FDS;

		obj.hdr.type = BINDER_TYPE_BINDER;
		obj.flags = 0x7f | FLAT_BINDER_FLAG_ACCEPTS_FDS;
		obj.binder = (uintptr_t)NULL;
		obj.cookie = 0;

		writebuf.txn.data_size = sizeof(struct flat_binder_object);
		writebuf.txn.offsets_size = sizeof(binder_size_t);

		writebuf.txn.data.ptr.buffer = (uintptr_t)&obj;
		writebuf.txn.data.ptr.offsets = (uintptr_t)&obj +
					sizeof(struct flat_binder_object);

		bwr.write_buffer = (uintptr_t)&writebuf;
		bwr.write_size = sizeof(writebuf);
		bwr.write_consumed = 0;

		if (verbose)
			printf("Service Provider sending transaction to Manager - ADD_TEST_SERVICE\n");

		/* Each test will expect a different number of replies */
		while (provider_replies) {
			bwr.read_size = sizeof(readbuf);
			bwr.read_consumed = 0;
			bwr.read_buffer = (uintptr_t)readbuf;

			result = ioctl(binder_fd, BINDER_WRITE_READ, &bwr);
			if (result < 0) {
				fprintf(stderr,
					"xxService Provider ioctl BINDER_WRITE_READ: %s\n",
					strerror(errno));
				result = 7;
				goto brexit;
			}

			if (verbose)
				printf("Service Provider read_consumed: %lld\n",
				       bwr.read_consumed);

			cmd = binder_parse(binder_fd, (uintptr_t)readbuf,
					   bwr.read_consumed, manager);

			if (cmd == BR_FAILED_REPLY ||
			    cmd == BR_DEAD_REPLY ||
			    cmd == BR_DEAD_BINDER) {
				result = 8;
				goto brexit;
			}
			provider_replies--;
		}
	}

brexit:
	free(context);
	munmap(shm_base, shm_size);
	close(shm_fd);
	munmap(map_base, map_size);
	close(binder_fd);

	return result;
}

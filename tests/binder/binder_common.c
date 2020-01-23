/*
 * This is a simple binder Service Manager/Service Provider that only uses
 * the raw ioctl commands to test the SELinux binder permissions:
 *     set_context_mgr, call, transfer, impersonate.
 *
 * If configured, the BPF permissions are also tested.
 *
 * Using binder test policy the following will be validated:
 *    security_binder_set_context_mgr() binder { set_context_mgr }
 *    security_binder_transaction()     binder { call impersonate }
 *    security_binder_transfer_binder() binder { transfer }
 *    security_binder_transfer_file()   fd { use }
 *					bpf { map_create map_read map_write };
 *					bpf { prog_load prog_run };
 */

#include "binder_common.h"

bool verbose;
enum binder_test_fd_t fd_type;
char *fd_type_str;

const char *cmd_name(uint32_t cmd)
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
#if HAVE_BINDERFS
	case BR_TRANSACTION_SEC_CTX:
		return "BR_TRANSACTION_SEC_CTX";
#endif
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
		return "Unknown Binder return code";
	}
}

void print_trans_data(const struct binder_transaction_data *txn_in)
{
	const struct flat_binder_object *obj;
	binder_size_t *offs = (binder_size_t *)
			      (binder_uintptr_t)txn_in->data.ptr.offsets;
	size_t count = txn_in->offsets_size / sizeof(binder_size_t);

	printf("\thandle: %d\n", (uint32_t)txn_in->target.handle);
	printf("\tcookie: %lld\n", txn_in->cookie);
	switch (txn_in->code) {
	case TEST_SERVICE_ADD:
		printf("\tcode: TEST_SERVICE_ADD\n");
		break;
	case TEST_SERVICE_GET:
		printf("\tcode: TEST_SERVICE_GET\n");
		break;
	case TEST_SERVICE_SEND_FD:
		printf("\tcode: TEST_SERVICE_SEND_FD\n");
		break;
	default:
		printf("Unknown binder_transaction_data->code: %x\n",
		       txn_in->code);
		return;
	}

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
		obj = (const struct flat_binder_object *)
		      (((char *)(binder_uintptr_t)txn_in->data.ptr.buffer) + *offs++);

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
#if HAVE_BINDERFS
		printf("\tflags: priority: 0x%x accept FDS: %s request security CTX: %s\n",
		       obj->flags & FLAT_BINDER_FLAG_PRIORITY_MASK,
		       obj->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS ? "YES" : "NO",
		       obj->flags & FLAT_BINDER_FLAG_TXN_SECURITY_CTX ? "YES" : "NO");
#else
		printf("\tflags: priority: 0x%x accept FDS: %s\n",
		       obj->flags & FLAT_BINDER_FLAG_PRIORITY_MASK,
		       obj->flags & FLAT_BINDER_FLAG_ACCEPTS_FDS ? "YES" : "NO");
#endif
		printf("\tcookie: %llx\n", obj->cookie);
	}
	fflush(stdout);
}

int binder_write(int fd, void *data, size_t len)
{
	struct binder_write_read bwr;
	int result;

	bwr.write_size = len;
	bwr.write_consumed = 0;
	bwr.write_buffer = (binder_uintptr_t)data;
	bwr.read_size = 0;
	bwr.read_consumed = 0;
	bwr.read_buffer = 0;

	result = ioctl(fd, BINDER_WRITE_READ, &bwr);
	if (result < 0) {
		fprintf(stderr, "binder_write ioctl BINDER_WRITE_READ error: %s\n",
			strerror(errno));
	}
	return result;
}

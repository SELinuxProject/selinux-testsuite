#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <infiniband/verbs.h>

struct ibv_qp	   *qp;
struct ibv_context *context;
struct ibv_pd      *pd;
struct ibv_cq      *cq;
struct ibv_srq     *srq;

void cleanup_ib_rsrc()
{
	ibv_destroy_qp(qp);
	ibv_destroy_srq(srq);
	ibv_destroy_cq(cq);
	ibv_dealloc_pd(pd);
	ibv_close_device(context);
}

int init_ib_rsrc(char *deviceName)
{
	int                 ndev = 0;
	struct ibv_device  **dlist = ibv_get_device_list(&ndev);
	struct ibv_device  *device = NULL;;
	struct ibv_srq_init_attr srqiattr;
	struct ibv_qp_init_attr qpiattr;
	int i;

	if (!ndev) {
		fprintf(stderr, "No IB devices found.\n");
		exit(1);
	}

	for (i = 0; i < ndev; i++)
		if(!strcmp(deviceName, dlist[i]->name))
			device = dlist[i];

	if (!device) {
		fprintf(stderr, "Couldn't find device %s\n", deviceName);
		exit(1);
	}
	/* Open context */
	context = ibv_open_device(device);
	if (NULL == context) {
		fprintf(stderr, "Unable to open device.\n");
		exit(1);
	}

	/* Allocate PD */
	pd = ibv_alloc_pd(context);
	if (!pd) {
		fprintf(stderr, "Unable to allocate PD.\n");
		exit(1);
	}

	/* Create CQ */
	cq = ibv_create_cq(context, 2048, NULL, NULL, 0);
	if (!cq) {
		fprintf(stderr, "Unable to create cq.\n");
		exit(1);
	}

	/* Create SRQ */
	memset(&srqiattr, 0, sizeof(srqiattr));
	srqiattr.attr.max_wr    = 2048;
	srqiattr.attr.max_sge   = 4;
	srqiattr.attr.srq_limit = 1024;
	srq = ibv_create_srq(pd, &srqiattr);
	if (NULL == srq) {
		fprintf(stderr, "Unable to create sreq.\n");
		exit(1);
	}

	memset(&qpiattr, 0, sizeof(qpiattr));
	qpiattr.send_cq = cq;
	qpiattr.recv_cq = cq;
	qpiattr.srq     = srq;
	qpiattr.cap.max_send_wr = 128;
	qpiattr.cap.max_recv_wr = 4;
	qpiattr.cap.max_send_sge = 5;
	qpiattr.cap.max_recv_sge = 4;
	qpiattr.cap.max_inline_data = 512;
	qpiattr.qp_type = IBV_QPT_RC;
	qpiattr.sq_sig_all = 1;
	qp = ibv_create_qp(pd, &qpiattr);

	if (!qp) {
		fprintf(stderr, "Unable to create QP %d.\n", i);
		exit(1);
	}

	return 0;
}

int init_rc_qp(uint8_t port, uint16_t pkey_index)
{
	struct ibv_qp_attr attr = {
		.qp_state        = IBV_QPS_INIT,
		.pkey_index      = pkey_index,
		.port_num        = port,
		.qp_access_flags = 0
	};

	return ibv_modify_qp(qp, &attr,
			     IBV_QP_STATE |
			     IBV_QP_PKEY_INDEX |
			     IBV_QP_PORT |
			     IBV_QP_ACCESS_FLAGS);
}

int main(int argc, char *argv[])
{
	uint16_t pkey_index;
	uint8_t port;
	int ret;

	if (argc != 4) {
		printf("Please enter <ib device name> <port number> <pkey index>\n");
		exit(1);
	}
	port = atoi(argv[2]);
	pkey_index = atoi(argv[3]);

	init_ib_rsrc(argv[1]);

	ret = init_rc_qp(port, pkey_index);
	cleanup_ib_rsrc();
	exit(ret);
}

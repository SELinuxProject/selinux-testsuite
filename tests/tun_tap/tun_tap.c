#include "tun_common.h"

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-p] [-s ] [-v]\n"
		"Where:\n\t"
		"-p  Test TAP driver, default is TUN driver.\n\t"
		"-s  If -v, then show TUN/TAP Features.\n\t"
		"-v  Print information.\n", progname);
	exit(-1);
}

int main(int argc, char *argv[])
{
	char *context, *test_str;
	int opt, result, fd, bit, count, test;
	unsigned int features, f_switch;
	bool verbose = false, show = false;
	struct ifreq ifr;

	test = IFF_TUN;
	test_str = "TUN";

	while ((opt = getopt(argc, argv, "psv")) != -1) {
		switch (opt) {
		case 'p':
			test = IFF_TAP;
			test_str = "TAP";
			break;
		case 's':
			show = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			print_usage(argv[0]);
		}
	}

	if (verbose) {
		result = getcon(&context);
		if (result < 0) {
			fprintf(stderr, "Failed to obtain process context\n");
			exit(-1);
		}

		printf("Process context:\n\t%s\n", context);
		free(context);
	}

	/* Start TUN/TAP */
	result = open_dev(&fd, test_str, verbose);
	if (result != 0)
		exit(result);

	if (verbose && show) {
		result = ioctl(fd, TUNGETFEATURES, &features);
		if (result < 0) {
			fprintf(stderr, "Failed ioctl(TUNGETFEATURES): %s\n",
				strerror(errno));
			exit(-1);
		}

		bit = 1;
		count = 0xffff;
		printf("TUN/TAP supported features:\n");
		while (count) {
			f_switch = bit & features;
			switch (f_switch) {
			case IFF_TUN:
				printf("\tIFF_TUN\n");
				break;
			case IFF_TAP:
				printf("\tIFF_TAP\n");
				break;
			case IFF_NAPI:
				printf("\tIFF_NAPI\n");
				break;
			case IFF_NAPI_FRAGS:
				printf("\tIFF_NAPI_FRAGS\n");
				break;
			case IFF_NO_PI:
				printf("\tIFF_NO_PI\n");
				break;
			case IFF_ONE_QUEUE:
				printf("\tIFF_ONE_QUEUE\n");
				break;
			case IFF_VNET_HDR:
				printf("\tIFF_VNET_HDR\n");
				break;
			/* Added in kernel 3.8 */
			case IFF_MULTI_QUEUE:
				printf("\tIFF_MULTI_QUEUE\n");
				break;
			}
			bit <<= 1;
			count >>= 1;
		}
	}

	/* Let TUNSETIFF allocate the ifr_name */
	memset(&ifr, 0, sizeof(struct ifreq));
	ifr.ifr_flags = test | IFF_MULTI_QUEUE;
	result = setiff(fd, &ifr, verbose);
	if (result != 0)
		goto err;

	/*
	 * Test 'tun_socket { attach_queue }' permission by removing
	 * the queue. Adding it back then generates the call to:
	 *     selinux_tun_dev_attach_queue().
	 */
	result = tunsetqueue(fd, 0, ifr.ifr_name, verbose);
	if (result != 0)
		goto err;

	result = tunsetqueue(fd, 1, ifr.ifr_name, verbose);
err:
	close(fd);
	return result;
}

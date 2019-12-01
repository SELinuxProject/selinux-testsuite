#include "tun_common.h"

int open_dev(int *fd, char *test_str, bool verbose)
{
	char *tun_dev = "/dev/net/tun";

	*fd = open(tun_dev, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed to open device: %s\n",
			strerror(errno));
		return errno;
	}
	if (verbose)
		printf("Opened: %s and testing %s service.\n",
		       tun_dev, test_str);

	return 0;
}

int setiff(int fd, struct ifreq *ifr, bool verbose)
{
	int result;

	result = ioctl(fd, TUNSETIFF, ifr);
	if (result < 0) {
		fprintf(stderr, "Failed ioctl(TUNSETIFF): %s\n",
			strerror(errno));
		return errno;
	}
	if (verbose)
		printf("ioctl(TUNSETIFF) name: %s\n", ifr->ifr_name);

	return 0;
}

int persist(int fd, int op, char *name, bool verbose)
{
	int result;

	result = ioctl(fd, TUNSETPERSIST, op);
	if (result < 0) {
		fprintf(stderr, "Failed ioctl(TUNSETPERSIST %s): %s\n",
			op ? "Set" : "Unset", strerror(errno));
		return errno;
	}
	if (verbose)
		printf("%s ioctl(TUNSETPERSIST) name: %s\n",
		       op ? "Set" : "Unset", name);

	return 0;
}

int tunsetqueue(int fd, int op, char *name, bool verbose)
{
	int result;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = (op ? IFF_ATTACH_QUEUE : IFF_DETACH_QUEUE);
	result = ioctl(fd, TUNSETQUEUE, &ifr);
	if (result < 0) {
		fprintf(stderr, "Failed ioctl(TUNSETQUEUE %s): %s\n",
			op ? "IFF_ATTACH_QUEUE" : "IFF_DETACH_QUEUE",
			strerror(errno));
		return errno;
	}
	if (verbose)
		printf("ioctl(TUNSETQUEUE) %s name: %s\n",
		       op ? "IFF_ATTACH_QUEUE" : "IFF_DETACH_QUEUE", name);

	return 0;
}

int switch_context(char *newcon, bool verbose)
{
	int result;

	result = setcon(newcon);
	if (result < 0) {
		fprintf(stderr, "setcon() failed to set new process context:\n\t%s\n",
			newcon);
		return -1;
	}

	if (verbose)
		printf("New process context:\n\t%s\n", newcon);

	free(newcon);

	return 0;
}

void del_tuntap_name(int fd, char *context, char *name, bool verbose)
{
	if (verbose)
		printf("Switching back to orig context to remove persistent name: %s\n",
		       name);
	switch_context(context, verbose);
	persist(fd, 0, name, verbose);
}

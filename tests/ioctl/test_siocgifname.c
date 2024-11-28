#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>

int main(int argc, char **argv)
{
	int sock;
	int rc;
	struct ifreq val = {};

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock == -1) {
		perror("test_siocgifname:socket");
		exit(1);
	}

	rc = ioctl(sock, SIOCGIFNAME, &val);
	if (rc < 0 && errno != ENODEV) {
		perror("test_siocgifname:SIOCGIFNAME");
		close(sock);
		exit(7);
	}

	close(sock);
	exit(0);
}

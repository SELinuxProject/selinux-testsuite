#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

int main(int argc, char *argv[])
{
	int i, rc;
	int fd;
	unsigned char data[512];
	struct nlmsghdr *nh[3];
	struct sockaddr_nl sa;
	struct iovec iov;
	struct msghdr msg;

	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;

	memset(data, 0, sizeof(data));
	iov.iov_base = data;
	iov.iov_len = 3 * NLMSG_SPACE(0);

	for (i = 0; i < 3; i++) {
		nh[i] = (struct nlmsghdr *)(data + (i * NLMSG_SPACE(0)));
		nh[i]->nlmsg_len = NLMSG_HDRLEN;
	}
	nh[0]->nlmsg_type = RTM_GETLINK; // nlmsg_read
	nh[1]->nlmsg_type = RTM_SETLINK; // nlmsg_write
	nh[2]->nlmsg_type = RTM_GETADDR; // nlmsg_read

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &sa;
	msg.msg_namelen = sizeof(sa);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	rc = sendmsg(fd, &msg, 0);

	if (rc < 0) {
		perror("sendmsg");
		exit(-1);
	}
	exit(0);
}


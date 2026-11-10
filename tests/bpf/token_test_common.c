#include "token_test_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/unistd.h>

inline int sys_fsopen(const char *fsname, unsigned int flags)
{
	return syscall(__NR_fsopen, fsname, flags);
}

inline int sys_mount(const char *dev_name, const char *dir_name,
		     const char *type, unsigned long flags,
		     const void *data)
{
	return syscall(__NR_mount, dev_name, dir_name, type, flags, data);
}

inline int sys_fsconfig(int fs_fd, unsigned int cmd, const char *key,
			const void *val, int aux)
{
	return syscall(__NR_fsconfig, fs_fd, cmd, key, val, aux);
}

inline int sys_fsmount(int fs_fd, unsigned int flags,
		       unsigned int ms_flags)
{
	return syscall(__NR_fsmount, fs_fd, flags, ms_flags);
}

static ssize_t write_nointr(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	do {
		ret = write(fd, buf, count);
	} while (ret < 0 && errno == EINTR);

	return ret;
}

int write_file(const char *path, const void *buf, size_t count)
{
	int fd;
	ssize_t ret;

	fd = open(path, O_WRONLY | O_CLOEXEC | O_NOCTTY | O_NOFOLLOW);
	if (fd < 0)
		return -1;

	ret = write_nointr(fd, buf, count);
	close(fd);
	if (ret < 0 || (size_t)ret != count)
		return -1;

	return 0;
}

int create_bpffs_fd(void)
{
	int fs_fd;

	/* create VFS context */
	fs_fd = sys_fsopen("bpf", 0);
	ASSERT_GE(fs_fd, 0, "fs_fd");

	return fs_fd;
}

int create_and_enter_userns(void)
{
	uid_t uid;
	gid_t gid;
	char map[100];

	uid = getuid();
	gid = getgid();

	if (unshare(CLONE_NEWUSER))
		return -1;

	if (write_file("/proc/self/setgroups", "deny", sizeof("deny") - 1) &&
	    errno != ENOENT)
		return -1;

	snprintf(map, sizeof(map), "0 %d 1", uid);
	if (write_file("/proc/self/uid_map", map, strlen(map)))
		return -1;

	snprintf(map, sizeof(map), "0 %d 1", gid);
	if (write_file("/proc/self/gid_map", map, strlen(map)))
		return -1;

	if (setgid(0))
		return -1;

	if (setuid(0))
		return -1;

	return 0;
}

int sendfd(int sockfd, int fd)
{
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	int fds[1] = {fd}, err;
	char iobuf[1];
	struct iovec io = {
		.iov_base = iobuf,
		.iov_len = sizeof(iobuf),
	};
	union {
		char buf[CMSG_SPACE(sizeof(fds))];
		struct cmsghdr align;
		} u;

	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fds));
	memcpy(CMSG_DATA(cmsg), fds, sizeof(fds));

	err = sendmsg(sockfd, &msg, 0);
	if (err < 0)
		err = -errno;
	if (!ASSERT_EQ(err, 1, "sendmsg"))
		return -EINVAL;

	return 0;
}

int recvfd(int sockfd, int *fd)
{
	struct msghdr msg = {};
	struct cmsghdr *cmsg;
	int fds[1], err;
	char iobuf[1];
	struct iovec io = {
		.iov_base = iobuf,
		.iov_len = sizeof(iobuf),
	};
	union {
		char buf[CMSG_SPACE(sizeof(fds))];
		struct cmsghdr align;
		} u;

	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);

	err = recvmsg(sockfd, &msg, 0);
	if (err < 0)
		err = -errno;
	if (!ASSERT_EQ(err, 1, "recvmsg"))
		return -EINVAL;

	cmsg = CMSG_FIRSTHDR(&msg);
	if (!ASSERT_OK_PTR(cmsg, "cmsg_null") ||
	    !ASSERT_EQ(cmsg->cmsg_len, CMSG_LEN(sizeof(fds)), "cmsg_len") ||
	    !ASSERT_EQ(cmsg->cmsg_level, SOL_SOCKET, "cmsg_level") ||
	    !ASSERT_EQ(cmsg->cmsg_type, SCM_RIGHTS, "cmsg_type"))
		return -EINVAL;

	memcpy(fds, CMSG_DATA(cmsg), sizeof(fds));
	*fd = fds[0];

	return 0;
}

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/dma-heap.h>

#define HEAP "/dev/dma_heap/system"

#ifndef __NR_pidfd_open
#define __NR_pidfd_open 434
#endif

/*
 * Returns 0 on success, -1 if the heap cannot be opened, otherwise
 * the errno of the failing ioctl.
 */
static int alloc_charged(int charge_fd)
{
	struct dma_heap_allocation_data data = {
		.len = 4096,
		.fd_flags = O_RDWR | O_CLOEXEC,
		.charge_fd = charge_fd,
	};
	int heap_fd, err;

	heap_fd = open(HEAP, O_RDWR | O_CLOEXEC);
	if (heap_fd < 0) {
		perror("open " HEAP);
		return -1;
	}

	if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) {
		err = errno;
		perror("DMA_HEAP_IOCTL_ALLOC");
		close(heap_fd);
		return err;
	}

	close(data.fd);
	close(heap_fd);
	return 0;
}

/* Returns 0 if allowed, 2 if denied (EACCES), 1 otherwise. */
static int status(int err)
{
	if (!err)
		return 0;
	return err == EACCES ? 2 : 1;
}

static void print_usage(char *progname)
{
	fprintf(stderr, "Usage: %s probe | pid | cgroup <dir>\n", progname);
	exit(1);
}

int main(int argc, char *argv[])
{
	int fd, err;

	if (argc < 2 || argc > 3) {
		print_usage(argv[0]);
	}

	if (argc == 2 && strcmp(argv[1], "probe") == 0) {
		/*
		 * A kernel without charge_fd support ignores the field and the
		 * allocation succeeds. One that has it rejects an fd that is
		 * neither a pidfd nor a cgroupfd.
		 */
		fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
		if (fd < 0) {
			perror("/dev/null");
			exit(1);
		}
		exit(alloc_charged(fd) == EBADF ? 0 : 1);
	}

	if (argc == 2 && strcmp(argv[1], "pid") == 0) {
		fd = syscall(__NR_pidfd_open, getpid(), 0);
		if (fd < 0) {
			perror("pidfd_open");
			exit(1);
		}
	} else if (argc == 3 && strcmp(argv[1], "cgroup") == 0) {
		fd = open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (fd < 0) {
			perror(argv[2]);
			exit(1);
		}
	} else {
		print_usage(argv[0]);
	}

	err = alloc_charged(fd);
	close(fd);
	exit(status(err));
}

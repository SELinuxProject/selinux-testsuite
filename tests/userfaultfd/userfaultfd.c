#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/xattr.h>

#include <linux/userfaultfd.h>

int page_size;

void *fault_handler_thread(void *arg)
{
	long uffd = (long)arg;
	struct uffd_msg msg = {0};
	struct uffdio_copy uffdio_copy = {0};
	ssize_t nread;
	char *page = (char *) mmap(NULL, page_size,  PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (page == MAP_FAILED) {
		perror("mmap");
		exit(-1);
	}
	memset(page, 'a', page_size);

	// Loop, handling incoming events on the userfaultfd file descriptor
	for (;;) {
		// poll on uffd waiting for an event
		struct pollfd pollfd;
		int nready;
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		nready = poll(&pollfd, 1, -1);
		if (nready == -1) {
			perror("poll");
			exit(-1);
		}

		/* Read an event from the userfaultfd */
		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			printf("EOF on userfaultfd!\n");
			exit(-1);
		}

		if (nread == -1) {
			if (errno == EACCES) {
				exit(6);
			}
			perror("read");
			exit(-1);
		}

		// We expect only one kind of event; verify that assumption
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(-1);
		}

		uffdio_copy.src = (unsigned long) page;

		// Align fault address to page boundary
		uffdio_copy.dst = (unsigned long) msg.arg.pagefault.address &
				  ~(page_size - 1);
		uffdio_copy.len = page_size;
		uffdio_copy.mode = 0; // Wake-up thread thread waiting for page-fault resolution
		uffdio_copy.copy = 0; // Used by kernel to return how many bytes copied
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) < 0) {
			if (errno == EACCES) {
				exit(7);
			}
			perror("ioctl-UFFDIO_COPY");
			exit(-1);
		}
	}
}

int main (int argc, char *argv[])
{
	char *addr;
	struct uffdio_api api = {0};
	struct uffdio_register uffdio_register = {0};
	char selinux_ctxt[128];
	pthread_t thr; // ID of thread that handles page faults
	ssize_t ret;

	long uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd < 0) {
		if (errno == ENOSYS) {
			return 8;
		} else if (errno == EACCES) {
			return 1;
		}
		perror("syscall(userfaultfd)");
		return -1;
	}

	// Check security context of uffd
	ret = fgetxattr(uffd, "security.selinux", selinux_ctxt, 128);
	if (ret < 0) {
		if (errno == EOPNOTSUPP) {
			return 8;
		} else if (errno == EACCES) {
			return 2;
		}
		perror("fgetxattr");
		return -1;
	}
	selinux_ctxt[ret] = 0;
	if (strstr(selinux_ctxt, argv[1]) == NULL) {
		fprintf(stderr, "Couldn't find the right selinux context. "
			"got:%s expected:%s\n", selinux_ctxt, argv[1]);
		return 3;
	}

	api.api = UFFD_API;
	if (ioctl(uffd, UFFDIO_API, &api) < 0) {
		if (errno == EACCES) {
			return 4;
		}
		perror("UFFDIO_API");
		return -1;
	}

	page_size = sysconf(_SC_PAGE_SIZE);
	/* Create a private anonymous mapping. The memory will be
	 * demand-zero paged--that is, not yet allocated. When we
	 * actually touch the memory, it will be allocated via
	 * the userfaultfd.
	 */
	addr = (char *) mmap(NULL, page_size, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	/* Register the memory range of the mapping we just created for
	 * handling by the userfaultfd object. In mode, we request to track
	 * missing pages (i.e., pages that have not yet been faulted in).
	 */
	uffdio_register.range.start = (unsigned long) addr;
	uffdio_register.range.len = page_size;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) < 0) {
		if (errno == EACCES) {
			return 5;
		}
		perror("ioctl-UFFDIO_REGISTER");
		return -1;
	}

	// Create a thread that will process the userfaultfd events
	ret = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (ret != 0) {
		errno = ret;
		perror("pthread_create");
		return -1;
	}

	/* Acces to the registered memory range should invoke the 'missing'
	 * userfaultfd page fault, which should get handled by the thread
	 * created above.
	 */
	if (addr[42] != 'a') {
		fprintf(stderr, "Didn't read the expected value after userfaultfd event\n");
		return -1;
	}

	return 0;
}

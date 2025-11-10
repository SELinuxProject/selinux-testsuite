/*
 * Copied from
 * https://github.com/linux-audit/audit-testsuite/tree/main/tests/io_uring
 * with the minimal changes required to make it work as part of the
 * selinux-testsuite.
 *
 * io_uring test tool to exercise LSM/SELinux and audit kernel code paths
 * Author: Paul Moore <paul@paul-moore.com>
 *
 * Copyright (c) 2021 Microsoft Corporation <paulmoore@microsoft.com>
 *
 * At the time this code was written the best, and most current, source of info
 * on io_uring seemed to be the liburing sources themselves (link below).  The
 * code below is based on the lessons learned from looking at the liburing
 * code.
 *
 * -> https://github.com/axboe/liburing
 *
 * The liburing LICENSE file contains the following:
 *
 * Copyright 2020 Jens Axboe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

/*
 * BUILDING:
 *
 * gcc -o <binary> -g -O0 -luring -lrt <source>
 *
 * RUNNING:
 *
 * The program can be run using the following command lines:
 *
 *  % prog sqpoll
 * ... this invocation runs the io_uring SQPOLL test.
 *
 *  % prog t1
 * ... this invocation runs the parent/child io_uring sharing test.
 *
 *  % prog t1 <domain>
 * ... this invocation runs the parent/child io_uring sharing test with the
 * child process run in the specified SELinux domain.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <liburing.h>
#include <libgen.h>

struct urt_config {
	struct io_uring ring;
	struct io_uring_params ring_params;
	int ring_creds;
};

#define URING_ENTRIES				8
#define URING_SHM_NAME				"/iouring_test"

int selinux_state = -1;
#define SELINUX_CTX_MAX				4096
char selinux_ctx[SELINUX_CTX_MAX] = "\0";

char uring_path[PATH_MAX];

/**
 * Display an error message and exit
 * @param msg the error message
 *
 * Output @msg to stderr and exit with errno as the exit value.
 */
void fatal(const char *msg)
{
	const char *str = (msg ? msg : "unknown");
	int save_errno = errno; // save before any potential reset below

	if (!save_errno) {
		save_errno = 1;
		fprintf(stderr, "%s: unknown error\n", msg);
	} else
		perror(str);

	shm_unlink(URING_SHM_NAME);

	if (save_errno < 0)
		exit(-save_errno);
	exit(save_errno);
}

/**
 * Determine if SELinux is enabled and set the internal state
 *
 * Attempt to read from /proc/self/attr/current and determine if SELinux is
 * enabled, store the current context/domain in @selinux_ctx if SELinux is
 * enabled.  We avoid using the libselinux API in order to increase portability
 * and make it easier for other LSMs to adopt this test.
 */
int selinux_enabled(void)
{
	int fd = -1;
	ssize_t ctx_len;
	char ctx[SELINUX_CTX_MAX];

	if (selinux_state >= 0)
		return selinux_state;

	/* attempt to get the current context */
	fd = open("/proc/self/attr/current", O_RDONLY);
	if (fd < 0)
		goto err;
	ctx_len = read(fd, ctx, SELINUX_CTX_MAX - 1);
	if (ctx_len <= 0)
		goto err;
	close(fd);

	/* save the current context */
	ctx[ctx_len] = '\0';
	strcpy(selinux_ctx, ctx);

	selinux_state = 1;
	return selinux_state;

err:
	if (fd >= 0)
		close(fd);

	selinux_state = 0;
	return selinux_state;
}

/**
 * Return the current SELinux domain or "DISABLED" if SELinux is not enabled
 *
 * The returned string should not be free()'d.
 */
const char *selinux_current(void)
{
	int rc;

	rc = selinux_enabled();
	if (!rc)
		return "DISABLED";

	return selinux_ctx;
}

/**
 * Set the SELinux domain for the next exec()'d process
 * @param ctx the SELinux domain
 *
 * This is similar to the setexeccon() libselinux API but we do it manually to
 * help increase portability and make it easier for other LSMs to adopt this
 * test.
 */
int selinux_exec(const char *ctx)
{
	int fd = -1;
	ssize_t len;

	if (!ctx)
		return -EINVAL;

	fd = open("/proc/self/attr/exec", O_WRONLY);
	if (fd < 0)
		return -errno;
	len = write(fd, ctx, strlen(ctx) + 1);
	close(fd);

	return len;
}

/**
 * Setup the io_uring
 * @param ring the io_uring pointer
 * @param params the io_uring parameters
 * @param creds pointer to the current process' registered io_uring personality
 *
 * Create a new io_uring using @params and return it in @ring with the
 * registered personality returned in @creds.  Returns 0 on success, negative
 * values on failure.
 */
int uring_setup(struct io_uring *ring,
		struct io_uring_params *params, int *creds)
{
	int rc;

	/* call into liburing to do the setup heavy lifting */
	rc = io_uring_queue_init_params(URING_ENTRIES, ring, params);
	if (rc < 0)
		fatal("io_uring_queue_init_params");

	/* register our creds/personality */
	rc = io_uring_register_personality(ring);
	if (rc < 0)
		fatal("io_uring_register_personality()");
	*creds = rc;
	rc = 0;

	printf(">>> io_uring created; fd = %d, personality = %d\n",
	       ring->ring_fd, *creds);

	return rc;
}

/**
 * Import an existing io_uring based on the given file descriptor
 * @param fd the io_uring's file descriptor
 * @param ring the io_uring pointer
 * @param params the io_uring parameters
 *
 * This function takes an io_uring file descriptor in @fd as well as the
 * io_uring parameters in @params and creates a valid io_uring in @ring.
 * Returns 0 on success, negative values on failure.
 */
int uring_import(int fd, struct io_uring *ring, struct io_uring_params *params)
{
	memset(ring, 0, sizeof(*ring));
	ring->flags = params->flags;
	ring->features = params->features;
	ring->ring_fd = fd;
	ring->enter_ring_fd = fd;

	ring->sq.ring_sz = params->sq_off.array +
			   params->sq_entries * sizeof(unsigned);
	ring->cq.ring_sz = params->cq_off.cqes +
			   params->cq_entries * sizeof(struct io_uring_cqe);

	ring->sq.ring_ptr = mmap(NULL, ring->sq.ring_sz, PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_POPULATE, fd,
				 IORING_OFF_SQ_RING);
	if (ring->sq.ring_ptr == MAP_FAILED)
		fatal("import mmap(ring)");

	ring->cq.ring_ptr = mmap(0, ring->cq.ring_sz, PROT_READ | PROT_WRITE,
				 MAP_SHARED | MAP_POPULATE,
				 fd, IORING_OFF_CQ_RING);
	if (ring->cq.ring_ptr == MAP_FAILED) {
		ring->cq.ring_ptr = NULL;
		goto err;
	}

	ring->sq.khead = ring->sq.ring_ptr + params->sq_off.head;
	ring->sq.ktail = ring->sq.ring_ptr + params->sq_off.tail;
	ring->sq.kring_mask = ring->sq.ring_ptr + params->sq_off.ring_mask;
	ring->sq.kring_entries = ring->sq.ring_ptr +
				 params->sq_off.ring_entries;
	ring->sq.ring_mask = *ring->sq.kring_mask;
	ring->sq.ring_entries = *ring->sq.kring_entries;
	ring->sq.kflags = ring->sq.ring_ptr + params->sq_off.flags;
	ring->sq.kdropped = ring->sq.ring_ptr + params->sq_off.dropped;
	ring->sq.array = ring->sq.ring_ptr + params->sq_off.array;

	ring->sq.sqes = mmap(NULL,
			     params->sq_entries * sizeof(struct io_uring_sqe),
			     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
			     fd, IORING_OFF_SQES);
	if (ring->sq.sqes == MAP_FAILED)
		goto err;

	ring->cq.khead = ring->cq.ring_ptr + params->cq_off.head;
	ring->cq.ktail = ring->cq.ring_ptr + params->cq_off.tail;
	ring->cq.kring_mask = ring->cq.ring_ptr + params->cq_off.ring_mask;
	ring->cq.kring_entries = ring->cq.ring_ptr +
				 params->cq_off.ring_entries;
	ring->cq.ring_mask = *ring->sq.kring_mask;
	ring->cq.ring_entries = *ring->sq.kring_entries;
	ring->cq.koverflow = ring->cq.ring_ptr + params->cq_off.overflow;
	ring->cq.cqes = ring->cq.ring_ptr + params->cq_off.cqes;
	if (params->cq_off.flags)
		ring->cq.kflags = ring->cq.ring_ptr + params->cq_off.flags;

	return 0;

err:
	if (ring->sq.ring_ptr)
		munmap(ring->sq.ring_ptr, ring->sq.ring_sz);
	if (ring->cq.ring_ptr)
		munmap(ring->cq.ring_ptr, ring->cq.ring_sz);
	fatal("import mmap");
	return -ENOMEM;
}

void uring_shutdown(struct io_uring *ring)
{
	if (!ring)
		return;
	io_uring_queue_exit(ring);
}

/**
 * An io_uring test
 * @param ring the io_uring pointer
 * @param personality the registered personality to use or 0
 * @param path the file path to use for the test
 *
 * This function executes an io_uring test, see the function body for more
 * details.  Returns 0 on success, negative values on failure.
 */
int uring_op_a(struct io_uring *ring, int personality, const char *path)
{

#define __OP_A_BSIZE		512
#define __OP_A_STR		"Lorem ipsum dolor sit amet.\n"

	int rc;
	int fds[1];
	char buf1[__OP_A_BSIZE];
	char buf2[__OP_A_BSIZE];
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	int str_sz = strlen(__OP_A_STR);

	memset(buf1, 0, __OP_A_BSIZE);
	memset(buf2, 0, __OP_A_BSIZE);
	strncpy(buf1, __OP_A_STR, str_sz);

	if (personality > 0)
		printf(">>> io_uring ops using personality = %d\n",
		       personality);

	/*
	 * open
	 */

	sqe = io_uring_get_sqe(ring);
	if (!sqe)
		fatal("io_uring_get_sqe(open)");
	io_uring_prep_openat(sqe, AT_FDCWD, path,
			     O_RDWR | O_TRUNC | O_CREAT, 0644);
	if (personality > 0)
		sqe->personality = personality;

	rc = io_uring_submit(ring);
	if (rc < 0)
		fatal("io_uring_submit(open)");

	rc = io_uring_wait_cqe(ring, &cqe);
	fds[0] = cqe->res;
	if (rc < 0)
		fatal("io_uring_wait_cqe(open)");
	if (fds[0] < 0)
		fatal("uring_open");
	io_uring_cqe_seen(ring, cqe);

	rc = io_uring_register_files(ring, fds, 1);
	if(rc)
		fatal("io_uring_register_files");

	printf(">>> io_uring open(): OK\n");

	/*
	 * write
	 */

	sqe = io_uring_get_sqe(ring);
	if (!sqe)
		fatal("io_uring_get_sqe(write1)");
	io_uring_prep_write(sqe, 0, buf1, str_sz, 0);
	io_uring_sqe_set_flags(sqe, IOSQE_FIXED_FILE);
	if (personality > 0)
		sqe->personality = personality;

	rc = io_uring_submit(ring);
	if (rc < 0)
		fatal("io_uring_submit(write)");

	rc = io_uring_wait_cqe(ring, &cqe);
	if (rc < 0)
		fatal("io_uring_wait_cqe(write)");
	if (cqe->res < 0)
		fatal("uring_write");
	if (cqe->res != str_sz)
		fatal("uring_write(length)");
	io_uring_cqe_seen(ring, cqe);

	printf(">>> io_uring write(): OK\n");

	/*
	 * read
	 */

	sqe = io_uring_get_sqe(ring);
	if (!sqe)
		fatal("io_uring_get_sqe(read1)");
	io_uring_prep_read(sqe, 0, buf2, __OP_A_BSIZE, 0);
	io_uring_sqe_set_flags(sqe, IOSQE_FIXED_FILE);
	if (personality > 0)
		sqe->personality = personality;

	rc = io_uring_submit(ring);
	if (rc < 0)
		fatal("io_uring_submit(read)");

	rc = io_uring_wait_cqe(ring, &cqe);
	if (rc < 0)
		fatal("io_uring_wait_cqe(read)");
	if (cqe->res < 0)
		fatal("uring_read");
	if (cqe->res != str_sz)
		fatal("uring_read(length)");
	io_uring_cqe_seen(ring, cqe);

	if (strncmp(buf1, buf2, str_sz))
		fatal("strncmp(buf1,buf2)");

	printf(">>> io_uring read(): OK\n");

	/*
	 * close
	 */

	sqe = io_uring_get_sqe(ring);
	if (!sqe)
		fatal("io_uring_get_sqe(close)");
	io_uring_prep_close(sqe, 0);
	if (personality > 0)
		sqe->personality = personality;

	rc = io_uring_submit(ring);
	if (rc < 0)
		fatal("io_uring_submit(close)");

	rc = io_uring_wait_cqe(ring, &cqe);
	if (rc < 0)
		fatal("io_uring_wait_cqe(close)");
	if (cqe->res < 0)
		fatal("uring_close");
	io_uring_cqe_seen(ring, cqe);

	rc = io_uring_unregister_files(ring);
	if (rc < 0)
		fatal("io_uring_unregister_files");

	printf(">>> io_uring close(): OK\n");

	return 0;
}

/**
 * The main entrypoint to the test program
 * @param argc number of command line options
 * @param argv the command line options array
 */
int main(int argc, char *argv[])
{
	int rc = 1;
	int ring_shm_fd;
	struct io_uring ring_storage, *ring;
	struct urt_config *cfg_p;
	char *progname;
	char *progdir;

	enum { TST_UNKNOWN,
	       TST_SQPOLL,
	       TST_T1_PARENT, TST_T1_CHILD
	     } tst_method;

	/* parse the command line and do some sanity checks */
	tst_method = TST_UNKNOWN;
	if (argc >= 2) {
		if (!strcmp(argv[1], "sqpoll"))
			tst_method = TST_SQPOLL;
		else if (!strcmp(argv[1], "t1") ||
			 !strcmp(argv[1], "t1_parent"))
			tst_method = TST_T1_PARENT;
		else if (!strcmp(argv[1], "t1_child"))
			tst_method = TST_T1_CHILD;
	}
	if (tst_method == TST_UNKNOWN) {
		fprintf(stderr, "usage: %s <method> ... \n", argv[0]);
		exit(EINVAL);
	}

	progname = strdup(argv[0]);
	progdir = dirname(progname);

	snprintf(uring_path, sizeof(uring_path), "%s/iouring.out", progdir);

	/* simple header */
	printf(">>> running as PID = %d\n", getpid());
	printf(">>> LSM/SELinux = %s\n", selinux_current());
	printf(">>> uring_path = %s\n", uring_path);

	/*
	 * test setup (if necessary)
	 */
	if (tst_method == TST_SQPOLL || tst_method == TST_T1_PARENT) {
		/* create an io_uring and prepare it for optional sharing */
		int flags;

		/* create a shm segment to hold the io_uring info */
		ring_shm_fd = shm_open(URING_SHM_NAME, O_CREAT | O_RDWR,
				       S_IRUSR | S_IWUSR);
		if (ring_shm_fd < 0)
			fatal("shm_open(create)");

		rc = ftruncate(ring_shm_fd, sizeof(struct urt_config));
		if (rc < 0)
			fatal("ftruncate(shm)");

		cfg_p = mmap(NULL, sizeof(*cfg_p), PROT_READ | PROT_WRITE,
			     MAP_SHARED, ring_shm_fd, 0);
		if (!cfg_p)
			fatal("mmap(shm)");

		/* create the io_uring */
		memset(&cfg_p->ring, 0, sizeof(cfg_p->ring));
		memset(&cfg_p->ring_params, 0, sizeof(cfg_p->ring_params));
		if (tst_method == TST_SQPOLL)
			cfg_p->ring_params.flags |= IORING_SETUP_SQPOLL;
		rc = uring_setup(&cfg_p->ring, &cfg_p->ring_params,
				 &cfg_p->ring_creds);
		if (rc)
			fatal("uring_setup");
		ring = &cfg_p->ring;

		/* explicitly clear FD_CLOEXEC on the io_uring */
		flags = fcntl(cfg_p->ring.ring_fd, F_GETFD, 0);
		if (flags < 0)
			fatal("fcntl(ring_shm_fd,getfd)");
		flags &= ~FD_CLOEXEC;
		rc = fcntl(cfg_p->ring.ring_fd, F_SETFD, flags);
		if (rc)
			fatal("fcntl(ring_shm_fd,setfd)");
	} else if (tst_method == TST_T1_CHILD) {
		/* import a previously created and shared io_uring */

		/* open the existing shm segment with the io_uring info */
		ring_shm_fd = shm_open(URING_SHM_NAME, O_RDWR, 0);
		if (ring_shm_fd < 0)
			fatal("shm_open(existing)");
		cfg_p = mmap(NULL, sizeof(*cfg_p), PROT_READ | PROT_WRITE,
			     MAP_SHARED, ring_shm_fd, 0);
		if (!cfg_p)
			fatal("mmap(shm)");

		/* import the io_uring */
		ring = &ring_storage;
		rc = uring_import(cfg_p->ring.ring_fd,
				  ring, &cfg_p->ring_params);
		if (rc < 0)
			fatal("uring_import");
	}

	/*
	 * fork/exec a child process (if necessary)
	 */
	if (tst_method == TST_T1_PARENT) {
		pid_t pid;

		/* set the ctx for the next exec */
		if (argc >= 3) {
			printf(">>> set LSM/SELinux exec: %s\n",
			       (selinux_exec(argv[2]) > 0 ? "OK" : "FAILED"));
		}

		/* fork/exec */
		pid = fork();
		if (pid < 0)
			fatal("fork");
		if (!pid) {
			/* start the child */
			rc = execl(argv[0], argv[0], "t1_child", (char *)NULL);
			if (rc < 0)
				fatal("exec");
		} else {
			/* wait for the child to exit */
			int status;
			waitpid(pid, &status, 0);
			if (WIFEXITED(status))
				rc = WEXITSTATUS(status);
		}
	}

	/*
	 * run test(s)
	 */
	if (tst_method == TST_SQPOLL || tst_method == TST_T1_CHILD) {
		rc = uring_op_a(ring, cfg_p->ring_creds, uring_path);
		if (rc < 0)
			fatal("uring_op_a()");
	}

	/*
	 * cleanup
	 */
	if (tst_method == TST_SQPOLL || tst_method == TST_T1_PARENT) {
		printf(">>> shutdown\n");
		uring_shutdown(&cfg_p->ring);
		shm_unlink(URING_SHM_NAME);
	} else if (tst_method == TST_T1_CHILD) {
		shm_unlink(URING_SHM_NAME);
	}

	return rc;
}

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <selinux/selinux.h>

enum {
	PERF_FILE_MMAP,
	PERF_FILE,
	PERF_MMAP
} read_type;

static void print_usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-f|-m] [-v]\n"
		"Where:\n\t"
		"-f  Read perf_event info using read(2)\n\t"
		"-m  Read perf_event info using mmap(2)\n\t"
		"    Default is to use read(2) and mmap(2)\n\t"
		"-v  Print information\n", progname);
	exit(-1);
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
			    int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, hw_event, pid, cpu,
		       group_fd, flags);
}

int main(int argc, char **argv)
{
	int opt, result, page_size, mmap_size, fd;
	long long count;
	bool verbose = false;
	char *context;
	void *base;
	struct perf_event_attr pe_attr;
	struct perf_event_mmap_page *pe_page;

	read_type = PERF_FILE_MMAP;

	while ((opt = getopt(argc, argv, "fmv")) != -1) {
		switch (opt) {
		case 'f':
			read_type = PERF_FILE;
			break;
		case 'm':
			read_type = PERF_MMAP;
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

	/* Test perf_event { open cpu kernel tracepoint } */
	memset(&pe_attr, 0, sizeof(struct perf_event_attr));
	pe_attr.type = PERF_TYPE_HARDWARE | PERF_TYPE_TRACEPOINT;
	pe_attr.size = sizeof(struct perf_event_attr);
	pe_attr.config = PERF_COUNT_HW_INSTRUCTIONS;
	pe_attr.disabled = 1;
	pe_attr.exclude_hv = 1;

	fd = perf_event_open(&pe_attr, -1, 1, -1, 0);
	if (fd < 0) {
		fprintf(stderr, "Failed perf_event_open(): %s\n",
			strerror(errno));
		if (errno == EACCES)
			exit(1);
		else
			exit(-1);
	}

	/* Test perf_event { write }; */
	result = ioctl(fd, PERF_EVENT_IOC_RESET, 0);
	if (result < 0) {
		fprintf(stderr, "Failed ioctl(PERF_EVENT_IOC_RESET): %s\n",
			strerror(errno));
		if (errno == EACCES)
			result = 2;
		goto err;
	}

	result = ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
	if (result < 0) {
		fprintf(stderr, "Failed ioctl(PERF_EVENT_IOC_ENABLE): %s\n",
			strerror(errno));
		if (errno == EACCES)
			result = 3;
		goto err;
	}

	result = ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
	if (result < 0) {
		fprintf(stderr, "Failed ioctl(PERF_EVENT_IOC_DISABLE): %s\n",
			strerror(errno));
		if (errno == EACCES)
			result = 4;
		goto err;
	}

	/* Test mmap(2) perf_event { read } */
	if (read_type == PERF_MMAP || read_type == PERF_FILE_MMAP) {
		page_size = sysconf(_SC_PAGESIZE);
		if (page_size < 0) {
			fprintf(stderr, "Failed sysconf(_SC_PAGESIZE): %s\n",
				strerror(errno));
			if (errno == EACCES)
				result = 5;
			else
				result = -1;
			goto err;
		}
		mmap_size = page_size * 2;

		base = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
			    MAP_SHARED, fd, 0);
		if (base == MAP_FAILED) {
			fprintf(stderr, "Failed mmap(): %s\n", strerror(errno));
			if (errno == EACCES)
				result = 6;
			else
				result = -1;
			goto err;
		}

		if (verbose) {
			pe_page = base;
			printf("perf mmap(2) return value: %lld\n",
			       pe_page->offset);
		}

		munmap(base, mmap_size);
	}

	/* Test read(2) perf_event { read } */
	if (read_type == PERF_FILE || read_type == PERF_FILE_MMAP) {
		result = read(fd, &count, sizeof(long long));
		if (result < 0) {
			fprintf(stderr, "Failed read(): %s\n", strerror(errno));
			if (errno == EACCES)
				result = 7;
			goto err;
		}

		if (verbose)
			printf("perf read(2) return value: %lld\n", count);

		close(fd);
	}

	return 0;

err:
	close(fd);
	return result;
}

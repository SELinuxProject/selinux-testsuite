#include "bpf_common.h"

/*
 * v0.7 deprecates some functions in favor of a new API (introduced in v0.6).
 * Make this work with both to satisfy -Werror (and older distros).
 */
#if defined(LIBBPF_MAJOR_VERSION)
#if LIBBPF_MAJOR_VERSION > 0 || (LIBBPF_MAJOR_VERSION == 0 && \
	LIBBPF_MINOR_VERSION > 6)
#define USE_NEW_API
#endif
#endif

int create_bpf_map(void)
{
	int map_fd, key;
	long long value = 0;

#ifdef USE_NEW_API
	map_fd = bpf_map_create(BPF_MAP_TYPE_ARRAY, NULL, sizeof(key),
				sizeof(value), 256, NULL);
#else
	map_fd = bpf_create_map(BPF_MAP_TYPE_ARRAY, sizeof(key),
				sizeof(value), 256, 0);
#endif
	if (map_fd < 0) {
		fprintf(stderr, "Failed to create BPF map: %s\n",
			strerror(errno));
		return -1;
	}

	return map_fd;
}

int create_bpf_prog(void)
{
	struct bpf_insn prog[] = {
		BPF_MOV64_IMM(BPF_REG_0, 1),
		BPF_EXIT_INSN(),
	};
	size_t insns_cnt = sizeof(prog) / sizeof(struct bpf_insn);
	int prog_fd;

#ifdef USE_NEW_API
	prog_fd = bpf_prog_load(BPF_PROG_TYPE_SOCKET_FILTER, NULL, "GPL",
				prog, insns_cnt, NULL);
#else
	prog_fd = bpf_load_program(BPF_PROG_TYPE_SOCKET_FILTER, prog,
				   insns_cnt, "GPL", 0, NULL, 0);
#endif

	if (prog_fd < 0) {
		fprintf(stderr, "Failed to load BPF prog: %s\n",
			strerror(errno));
		return -1;
	}

	return prog_fd;
}

/*
 * The default RLIMIT_MEMLOCK is normally 64K, however BPF map/prog requires
 * more than this (the actual threshold varying across arches) so set it to
 * RLIM_INFINITY.
 */
void bpf_setrlimit(void)
{
	int result;
	struct rlimit r;

	r.rlim_cur = RLIM_INFINITY;
	r.rlim_max = RLIM_INFINITY;

	result = setrlimit(RLIMIT_MEMLOCK, &r);
	if (result < 0) {
		fprintf(stderr, "Failed to set resource limit: %ld Err: %s\n",
			r.rlim_cur, strerror(errno));
		exit(-1);
	}
}

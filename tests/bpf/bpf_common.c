#include "bpf_common.h"

int create_bpf_map(void)
{
	int map_fd, key;
	long long value = 0;

	map_fd = bpf_create_map(BPF_MAP_TYPE_ARRAY, sizeof(key),
				sizeof(value), 256, 0);
	if (map_fd < 0) {
		fprintf(stderr, "Failed to create BPF map: %s\n",
			strerror(errno));
		return -1;
	}

	return map_fd;
}

int create_bpf_prog(void)
{
	int prog_fd;
	size_t insns_cnt;

	struct bpf_insn prog[] = {
		BPF_MOV64_IMM(BPF_REG_0, 1),
		BPF_EXIT_INSN(),
	};
	insns_cnt = sizeof(prog) / sizeof(struct bpf_insn);

	prog_fd = bpf_load_program(BPF_PROG_TYPE_SOCKET_FILTER, prog,
				   insns_cnt, "GPL", 0, NULL, 0);

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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <selinux/selinux.h>
#include <bpf/bpf.h>
#include <linux/bpf.h>
#include <sys/resource.h>

extern int create_bpf_map(void);
extern int create_bpf_prog(void);
extern void bpf_setrlimit(void);

/* edited eBPF instruction library */
/* Short form of mov, dst_reg = imm32 */
#define BPF_MOV64_IMM(DST, IMM)				\
	((struct bpf_insn) {				\
		.code  = BPF_ALU64 | BPF_MOV | BPF_K,	\
			 .dst_reg = DST,				\
				    .src_reg = 0,				\
					       .off   = 0,				\
							.imm   = IMM })

/* Program exit */
#define BPF_EXIT_INSN()				\
	((struct bpf_insn) {			\
		.code  = BPF_JMP | BPF_EXIT,	\
			 .dst_reg = 0,			\
				    .src_reg = 0,			\
					       .off   = 0,			\
							.imm   = 0 })


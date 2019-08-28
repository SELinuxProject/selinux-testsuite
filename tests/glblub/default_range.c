#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <selinux/selinux.h>

void run_test(const char *c1, const char *c2, security_class_t tclass,
	      const char *exp_con)
{
	char *buf = NULL;
	int ret;

	ret = security_compute_create(c1, c2, tclass, &buf);
	if (ret < 0) {
		exit(3);
	}

	if (exp_con == NULL && buf == NULL) {
		return;
	}

	if (exp_con == NULL && buf != NULL) {
		fprintf(stderr, "expected NULL, got %s\n", buf);
		freecon(buf);
		exit(3);
	}

	if (exp_con != NULL && buf == NULL) {
		fprintf(stderr, "expected %s, got NULL\n", exp_con);
		exit(3);
	}

	if (strcmp(buf, exp_con)) {
		fprintf(stderr, "%s did not match expected %s\n", buf, exp_con);
		exit(3);
	}

	freecon(buf);

}

int main(int argc, const char **argv)
{
	security_class_t tclass;
	const char *exp_con;

	if (argc != 4 && argc != 5) {
		fprintf(stderr, "Usage %s <source> <target> <class> [expected]\n", argv[0]);
		exit(1);
	}

	if (argc == 4) exp_con = NULL;
	else exp_con = argv[4];

	tclass = string_to_security_class(argv[3]);
	if (!tclass) {
		fprintf(stderr, "Invalid class '%s'\n", argv[3]);
		exit(1);
	}

	run_test(argv[1], argv[2], tclass, exp_con);

	exit(EXIT_SUCCESS);
}

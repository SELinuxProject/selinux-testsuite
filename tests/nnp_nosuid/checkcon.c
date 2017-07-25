#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <selinux/selinux.h>
#include <selinux/context.h>

int main(int argc, char **argv)
{
	char *con = NULL;
	context_t c;
	const char *type;
	int rc;

	if (argc != 2) {
		fprintf(stderr, "usage:  %s expected-type\n", argv[0]);
		exit(-1);
	}

	if (getcon(&con) < 0) {
		perror("getcon");
		exit(-1);
	}

	c = context_new(con);
	if (!c) {
		perror("context_new");
		exit(-1);
	}

	type = context_type_get(c);
	if (!type) {
		perror("context_type_get");
		exit(-1);

	}

	rc = strcmp(type, argv[1]);
	exit(rc);
}

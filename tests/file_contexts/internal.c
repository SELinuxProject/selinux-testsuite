#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <selinux/label.h>
#include <selinux/selinux.h>

#include "internal.h"

void assertContextsMatch(struct selabel_handle *hnd, const char *log_prefix,
			 struct test_t *tests, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		char *context = NULL;
		struct test_t test = tests[i];

		if (selabel_lookup(hnd, &context, test.path, S_IFREG)) {
			if (test.context) {
				log_errno("Lookup for %s from %s failed",
					  test.path, log_prefix);
				exit(2);
			}
			// Expected failure. Continue to the next test.
			continue;
		} else if (!test.context) {
			log_err("Lookup for %s from %s was supposed to failed but got %s",
				test.path, log_prefix, context);
			exit(2);
		}

		if (strcmp(context, tests[i].context)) {
			log_err("Lookup for %s from %s returned %s, expected %s",
				tests[i].path, log_prefix, context,
				tests[i].context);
			exit(2);
		}

		free(context);
	}
}

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(int argc, const char **argv)
{
	if (argc != 3 || (strcmp(argv[2], "R_OK") && strcmp(argv[2], "W_OK"))) {
		fprintf(stderr, "Usage %s <file> R_OK|W_OK\n", argv[0]);
		return EINVAL;
	}

	errno = 0;
	access(argv[1], strcmp(argv[2], "R_OK") == 0 ? R_OK : W_OK);
	return errno;
}

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

void handler(int sig)
{
	return;
}

int main(int argc, char **argv)
{
	struct sigaction sa;
	int i;
	FILE *f;

	if (argc != 2) {
		fprintf(stderr, "Need flag file argument!\n");
		exit(1);
	}

	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	for (i = 0; i < 32; i++) {
		sigaction(i, &sa, NULL);
	}

	f = fopen(argv[1], "w");
	if (!f) {
		perror("Flag file open");
		exit(1);
	}
	fprintf(f, "listening\n");
	fclose(f);

	while (1)
		;
}

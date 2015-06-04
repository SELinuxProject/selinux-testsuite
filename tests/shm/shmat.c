#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/shm.h>

int main(int argc, char **argv)
{
	int ch;
	int key = 0x8888;
	int id;
	int error;
	char *buf;

	while ((ch = getopt(argc, argv, "k:")) != -1) {
		switch (ch) {
		case 'k':
			key = atoi(optarg);
			break;
		}
	}

	id = shmget(key, 2048, IPC_CREAT | 0777);
	if (id == -1)
		return 1;

	buf = shmat(id, 0, 0);
	error = (buf == (void *) - 1) ? -1 : 0;
	printf("shmat: buf=%p, returned %d\n", buf, error);
	return error;
}

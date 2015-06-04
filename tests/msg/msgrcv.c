#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/msg.h>

#define MSGMAX 1024
struct msgbuf_test {
	long mtype;
	char mtext[MSGMAX];
};

int main(int argc, char **argv)
{
	int ch;
	int key = 0x8888;
	int id;
	int error;
	struct msgbuf_test msgp;

	while ((ch = getopt(argc, argv, "k:")) != EOF) {
		switch (ch) {
		case 'k':
			key = atoi(optarg);
			break;
		}
	}

	id = msgget(key, IPC_CREAT|0777);
	if (id == -1)
	  return 1;

	error = msgrcv(id, &msgp, MSGMAX, 1, IPC_NOWAIT);
	printf("msgrcv: result = %d\n", error);
	return error;
}

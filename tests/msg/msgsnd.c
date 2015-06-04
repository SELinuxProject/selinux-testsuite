#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/msg.h>
#include <string.h>

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

	id = msgget(key, IPC_CREAT | 0777);
	if (id == -1)
		return 1;

	memset(&msgp, 'z', sizeof(msgp));
	msgp.mtype = 1;

	error = msgsnd(id, &msgp, MSGMAX, IPC_NOWAIT);
	printf("msgsnd: error = %d\n", error);
	return error;
}

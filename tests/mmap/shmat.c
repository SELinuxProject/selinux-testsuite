#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>

int main(void)
{
	int shmid, rc = 0;
	char *execmem;

	shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0777);
	if (shmid < 0) {
		perror("shmget");
		exit(1);
	}
	execmem = shmat(shmid, 0, SHM_EXEC);
	if (execmem == ((void *) - 1)) {
		perror("shmat SHM_EXEC");
		rc = 1;
	} else {
		shmdt(execmem);
	}
	shmctl(shmid, IPC_RMID, 0);
	exit(rc);
}

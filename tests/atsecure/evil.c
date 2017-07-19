#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

uid_t getuid(void)
{
	printf("Evil code ran!\n");
	exit(1);
}

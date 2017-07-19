#include <sys/auxv.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
	return getauxval(AT_SECURE);

}

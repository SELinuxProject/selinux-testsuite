#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <keyutils.h>
#include <selinux/selinux.h>
#include <selinux/context.h>

/* This is used as the payload for each add_key() */
static const char payload[] =
	" -----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDN4FHsPjlJf03r9KfNt1Ma9/D6\nQDEiR/cfhZrNUPgHRresult+E4dj52VJSonPFJ6HaLlUi5pZq2t1LqPNrMfFKCNn12m+\nWw4aduBJM7u1RUPSNxrlfDAJZkdtNALOO/ds3U93hZrxOYNetzbnjILDu5JT1nbI\n4aC60SkdlCw1TxmvXwIDAQAB\n-----END PUBLIC KEY-----\n";

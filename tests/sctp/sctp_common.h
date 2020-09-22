#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* For poll(2) POLLRDHUP - Detect client close(2) */
#endif

#include <arpa/inet.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/sctp.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <selinux/selinux.h>

enum event_ret {
	EVENT_OK,
	EVENT_ADDR_MATCH,
	EVENT_SHUTDOWN,
	EVENT_NO_AUTH
};

void print_context(int fd, char *text);
void print_addr_info(struct sockaddr *sin, char *text);
char *get_ip_option(int fd, bool ipv4, socklen_t *opt_len);
void print_ip_option(int fd, bool ipv4, char *text);
int set_subscr_events(int fd, int data_io, int assoc, int addr, int shutd);
int handle_event(void *buf, char *cmp_addr, sctp_assoc_t *assoc_id,
		 bool verbose, char *text);

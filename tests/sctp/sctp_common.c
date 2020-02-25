#include "sctp_common.h"

#define member_size(type, member) sizeof(((type *)0)->member)
#define sizeof_up_to(type, member) (offsetof(type, member) + member_size(type, member))

void print_context(int fd, char *text)
{
	char *context;

	if (fgetfilecon(fd, &context) < 0)
		context = strdup("unavailable");
	printf("%s fd context: %s\n", text, context);
	free(context);

	if (getpeercon(fd, &context) < 0)
		context = strdup("unavailable");
	printf("%s peer context: %s\n", text, context);
	free(context);
}

void print_addr_info(struct sockaddr *sin, char *text)
{
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;
	char addr_str[INET6_ADDRSTRLEN + 1];

	switch (sin->sa_family) {
	case AF_INET:
		addr4 = (struct sockaddr_in *)sin;
		inet_ntop(sin->sa_family,
			  (void *)&addr4->sin_addr,
			  addr_str, INET6_ADDRSTRLEN + 1);
		printf("%s IPv4 addr %s\n", text, addr_str);
		break;
	case AF_INET6:
		addr6 = (struct sockaddr_in6 *)sin;
		if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
			inet_ntop(AF_INET,
				  (void *)&addr6->sin6_addr.s6_addr32[3],
				  addr_str, INET6_ADDRSTRLEN + 1);
			printf("%s IPv6->IPv4 MAPPED addr %s\n",
			       text, addr_str);
		} else {
			inet_ntop(sin->sa_family,
				  (void *)&addr6->sin6_addr,
				  addr_str, INET6_ADDRSTRLEN + 1);
			printf("%s IPv6 addr %s\n", text,
			       addr_str);
		}
		break;
	}
}

char *get_ip_option(int fd, bool ipv4, socklen_t *opt_len)
{
	int result, i;
	unsigned char ip_options[1024];
	socklen_t len = sizeof(ip_options);
	char *ip_optbuf;

	if (ipv4)
		result = getsockopt(fd, IPPROTO_IP, IP_OPTIONS,
				    ip_options, &len);
	else
		result = getsockopt(fd, IPPROTO_IPV6, IPV6_HOPOPTS,
				    ip_options, &len);

	if (result < 0) {
		perror("get ip options error");
		return NULL;
	}

	ip_optbuf = calloc(1, len * 2 + 1);
	if (!ip_optbuf) {
		perror("get ip options malloc error");
		return NULL;
	}

	if (len > 0) {
		for (i = 0; i < len; i++)
			sprintf(&ip_optbuf[i * 2], "%02x", ip_options[i]);

		*opt_len = len;
		return ip_optbuf;
	}

	return NULL;
}

void print_ip_option(int fd, bool ipv4, char *text)
{
	char *ip_options;
	socklen_t len;

	ip_options = get_ip_option(fd, ipv4, &len);

	if (ip_options) {
		printf("%s IP Options Family: %s Length: %d\n\tEntry: %s\n",
		       text, ipv4 ? "IPv4" : "IPv6", len, ip_options);
		free(ip_options);
	} else {
		printf("%s No IP Options set\n", text);
	}
}

int set_subscr_events(int fd, int data_io, int association)
{
	struct sctp_event_subscribe subscr_events;

	memset(&subscr_events, 0, sizeof(subscr_events));
	subscr_events.sctp_data_io_event = data_io;
	subscr_events.sctp_association_event = association;

	/*
	 * Truncate optlen to just the fields we touch to avoid errors when
	 * the uapi headers are newer than the running kernel.
	 */
	return setsockopt(fd, IPPROTO_SCTP, SCTP_EVENTS, &subscr_events,
			  sizeof_up_to(struct sctp_event_subscribe,
				       sctp_association_event));
}

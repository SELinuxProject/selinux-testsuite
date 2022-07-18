#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-4] [-b ipv4_addr] [-f file] [-h addr] [-n] [-v] stream|seq port\n"
		"\nWhere:\n\t"
		"-4      Listen on IPv4 addresses only (used for CIPSO tests).\n\t"
		"-b      Call sctp_bindx(3) with the supplied IPv4 address.\n\t"
		"-f      Write a line to the file when listening starts.\n\t"
		"-h      IPv4 or IPv6 listen address. If IPv6 link-local address,\n\t"
		"        then requires the %%<if_name> to obtain scopeid. e.g.\n\t"
		"            fe80::7629:afff:fe0f:8e5d%%wlp6s0\n\t"
		"-n      No peer label or IP option will be available therefore\n\t"
		"        send \"nopeer\" message to client.\n\t"
		"-v      Print context and ip options information.\n\t"
		"stream  Use SCTP 1-to-1 style or:\n\t"
		"seq     use SCTP 1-to-Many style.\n\t"
		"port    Listening port.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, sock, newsock, result, if_index = 0, on = 1, off = 0;
	socklen_t sinlen;
	struct sockaddr_storage sin;
	struct addrinfo hints, *res;
	struct sctp_sndrcvinfo sinfo;
	struct pollfd poll_fd;
	char getsockopt_peerlabel[1024];
	char byte, *peerlabel, msglabel[1024], if_name[30];
	bool nopeer = false,  verbose = false,  ipv4 = false;
	char *context, *host_addr = NULL, *bindx_addr = NULL, *flag_file = NULL;
	struct sockaddr_in ipv4_addr;
	unsigned short port;

	while ((opt = getopt(argc, argv, "4b:f:h:nv")) != -1) {
		switch (opt) {
		case '4':
			ipv4 = true;
			break;
		case 'b':
			bindx_addr = optarg;
			break;
		case 'f':
			flag_file = optarg;
			break;
		case 'h':
			host_addr = optarg;
			break;
		case 'n':
			nopeer = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}

	if ((argc - optind) != 2)
		usage(argv[0]);

	port = atoi(argv[optind + 1]);
	if (!port)
		usage(argv[0]);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_SCTP;

	if (ipv4)
		hints.ai_family = AF_INET;
	else
		hints.ai_family = AF_INET6;

	if (!strcmp(argv[optind], "stream"))
		hints.ai_socktype = SOCK_STREAM;
	else if (!strcmp(argv[optind], "seq"))
		hints.ai_socktype = SOCK_SEQPACKET;
	else
		usage(argv[0]);

	if (verbose) {
		if (getcon(&context) < 0)
			context = strdup("unavailable");
		printf("Server process context: %s\n", context);
		free(context);
	}

	if (host_addr) {
		char *ptr;

		ptr = strpbrk(host_addr, "%");
		if (ptr)
			strcpy(if_name, ptr + 1);

		if_index = if_nametoindex(if_name);
		if (!if_index) {
			perror("Server if_nametoindex");
			exit(1);
		}

		result = getaddrinfo(host_addr, argv[optind + 1],
				     &hints, &res);

	} else {
		result = getaddrinfo(NULL, argv[optind + 1], &hints, &res);
	}

	if (result < 0) {
		fprintf(stderr, "Server getaddrinfo: %s\n",
			gai_strerror(result));
		exit(1);
	}

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock < 0) {
		perror("Server socket");
		exit(1);
	}

	result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (result < 0) {
		perror("Server setsockopt: SO_REUSEADDR");
		close(sock);
		exit(1);
	}

	/* Enables sctp_data_io_events for sctp_recvmsg(3) for assoc_id. */
	result = set_subscr_events(sock, on, off, off, off);
	if (result < 0) {
		perror("Server setsockopt: SCTP_EVENTS");
		close(sock);
		exit(1);
	}

	if (bindx_addr) {
		memset(&ipv4_addr, 0, sizeof(struct sockaddr_in));
		ipv4_addr.sin_family = AF_INET;
		ipv4_addr.sin_port = htons(port);
		ipv4_addr.sin_addr.s_addr = inet_addr(bindx_addr);

		result = sctp_bindx(sock, (struct sockaddr *)&ipv4_addr, 1,
				    SCTP_BINDX_ADD_ADDR);
		if (result < 0) {
			perror("Server sctp_bindx ADD - ipv4");
			close(sock);
			exit(1);
		}
	} else {
		result = bind(sock, res->ai_addr, res->ai_addrlen);
		if (result < 0) {
			perror("Server bind");
			close(sock);
			exit(1);
		}
	}

	if (verbose) {
		print_context(sock, "Server LISTEN");
		print_ip_option(sock, ipv4, "Server LISTEN");
	}

	if (listen(sock, SOMAXCONN)) {
		perror("Server listen");
		close(sock);
		exit(1);
	}

	if (flag_file) {
		FILE *f = fopen(flag_file, "w");
		if (!f) {
			perror("Flag file open");
			exit(1);
		}
		fprintf(f, "listening\n");
		fclose(f);
	}

	if (hints.ai_socktype == SOCK_STREAM) {
		if (verbose)
			print_context(sock, "Server STREAM");

		do {
			socklen_t labellen = sizeof(getsockopt_peerlabel);

			sinlen = sizeof(sin);

			newsock = accept(sock, (struct sockaddr *)&sin,
					 &sinlen);
			if (newsock < 0) {
				perror("Server accept");
				close(sock);
				exit(1);
			}

			if (verbose) {
				print_context(newsock,
					      "Server STREAM accept on newsock");
				print_addr_info((struct sockaddr *)&sin,
						"Server connected to Client");
				print_ip_option(newsock, ipv4,
						"Server STREAM accept on newsock");
			}

			if (nopeer) {
				peerlabel = strdup("nopeer");
			} else {
				result = getpeercon(newsock, &peerlabel);
				if (result < 0) {
					perror("Server getpeercon");
					close(sock);
					close(newsock);
					exit(1);
				}

				/* Also test the getsockopt version */
				result = getsockopt(newsock, SOL_SOCKET,
						    SO_PEERSEC,
						    getsockopt_peerlabel,
						    &labellen);
				if (result < 0) {
					perror("Server getsockopt: SO_PEERSEC");
					close(sock);
					close(newsock);
					exit(1);
				}
				if (verbose)
					printf("Server STREAM SO_PEERSEC peer label: %s\n",
					       getsockopt_peerlabel);
			}
			printf("Server STREAM peer label: %s\n", peerlabel);

			result = read(newsock, &byte, 1);
			if (result < 0) {
				perror("Server read");
				close(sock);
				close(newsock);
				exit(1);
			}

			result = write(newsock, peerlabel, strlen(peerlabel));
			if (result < 0) {
				perror("Server write");
				close(sock);
				close(newsock);
				exit(1);
			}

			if (verbose)
				printf("Server STREAM sent: %s\n", peerlabel);

			free(peerlabel);

			/* Let the client close the connection first as this
			 * will stop OOTB chunks if newsock closed early.
			 */
			poll_fd.fd = newsock;
			poll_fd.events = POLLRDHUP;
			poll_fd.revents = 1;
			result = poll(&poll_fd, 1, 1000);
			if (verbose && result == 1)
				printf("Server STREAM: Client closed connection\n");
			else if (verbose && result == 0)
				printf("Server: poll(2) timed out - OKAY\n");
			else if (result < 0)
				perror("Server - poll");

			close(newsock);
		} while (1);
	} else { /* hints.ai_socktype == SOCK_SEQPACKET */
		if (verbose)
			print_context(sock, "Server SEQPACKET sock");

		do {
			sinlen = sizeof(sin);

			result = sctp_recvmsg(sock, msglabel, sizeof(msglabel),
					      (struct sockaddr *)&sin, &sinlen,
					      &sinfo, NULL);
			if (result < 0) {
				perror("Server sctp_recvmsg");
				close(sock);
				exit(1);
			}

			if (verbose) {
				print_context(sock, "Server SEQPACKET recvmsg");
				print_addr_info((struct sockaddr *)&sin,
						"Server SEQPACKET recvmsg");
				print_ip_option(sock, ipv4,
						"Server SEQPACKET recvmsg");
			}

			if (nopeer) {
				peerlabel = strdup("nopeer");
			} else {
				result = getpeercon(sock, &peerlabel);
				if (result < 0) {
					perror("Server getpeercon");
					close(sock);
					exit(1);
				}
			}
			printf("Server SEQPACKET peer label: %s\n", peerlabel);

			if (sin.ss_family == AF_INET6 && host_addr)
				((struct sockaddr_in6 *)&sin)->sin6_scope_id = if_index;

			result = sctp_sendmsg(sock, peerlabel,
					      strlen(peerlabel),
					      (struct sockaddr *)&sin,
					      sinlen, 0, 0, 0, 0, 0);
			if (result < 0) {
				perror("Server sctp_sendmsg");
				close(sock);
				exit(1);
			}

			if (verbose)
				printf("Server SEQPACKET sent: %s\n",
				       peerlabel);

			free(peerlabel);
		} while (1);
	}

	close(sock);
	exit(0);
}

#include "sctp_common.h"

static void usage(char *progname)
{
	fprintf(stderr,
		"usage:  %s [-4] [-f file] [-i] [-n] [-v] port\n"
		"\nWhere:\n\t"
		"-4      Listen on IPv4 addresses only.\n\t"
		"-f      Write a line to the file when listening starts.\n\t"
		"-i      Send IP Options as msg (default is peer label).\n\t"
		"-n      No peer context will be available therefore send\n\t"
		"        \"nopeer\" message to client, otherwise the peer context\n\t"
		"        will be retrieved and sent to client.\n\t"
		"-v      Print context and ip options information.\n\t"
		"port    Listening port.\n", progname);
	exit(1);
}

int main(int argc, char **argv)
{
	int opt, sock, result, peeloff_sk = 0, flags, on = 1, off = 0;
	sctp_assoc_t assoc_id = 0;
	socklen_t sinlen, opt_len;
	struct sockaddr_storage sin;
	struct addrinfo hints, *res;
	char *peerlabel, *context, *flag_file = NULL, msglabel[256];
	bool nopeer = false,  verbose = false, ipv4 = false, snd_opt = false;
	unsigned short port;

	while ((opt = getopt(argc, argv, "4f:inv")) != -1) {
		switch (opt) {
		case '4':
			ipv4 = true;
			break;
		case 'f':
			flag_file = optarg;
			break;
		case 'i':
			snd_opt = true;
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

	if ((argc - optind) != 1)
		usage(argv[0]);

	port = atoi(argv[optind]);
	if (!port)
		usage(argv[0]);

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_SCTP;

	if (ipv4)
		hints.ai_family = AF_INET;
	else
		hints.ai_family = AF_INET6;

	/* sctp_peeloff(3) must be from 1 to Many style socket */
	hints.ai_socktype = SOCK_SEQPACKET;

	if (verbose) {
		if (getcon(&context) < 0)
			context = strdup("unavailable");
		printf("Server process context: %s\n", context);
		free(context);
	}

	result = getaddrinfo(NULL, argv[optind], &hints, &res);
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

	result = bind(sock, res->ai_addr, res->ai_addrlen);
	if (result < 0) {
		perror("Server bind");
		close(sock);
		exit(1);
	}

	if (verbose)
		print_context(sock, "Server LISTEN sock");

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

	do {
		/* Get assoc_id for sctp_peeloff() */
		result = set_subscr_events(sock, off, on, off, off);
		if (result < 0) {
			perror("Server setsockopt: SCTP_EVENTS");
			close(sock);
			exit(1);
		}
		sinlen = sizeof(sin);
		flags = 0;

		result = sctp_recvmsg(sock, msglabel, sizeof(msglabel),
				      (struct sockaddr *)&sin, &sinlen,
				      NULL, &flags);
		if (result < 0) {
			perror("Server sctp_recvmsg-1");
			close(sock);
			exit(1);
		}

		if (verbose)
			print_addr_info((struct sockaddr *)&sin,
					"Server SEQPACKET recvmsg");

		if (flags & MSG_NOTIFICATION && flags & MSG_EOR) {
			handle_event(msglabel, NULL, &assoc_id,
				     verbose, "Peeloff Server");
			if (assoc_id <= 0) {
				printf("Server Invalid association ID: %d\n",
				       assoc_id);
				close(sock);
				exit(1);
			}
			/* No more notifications */
			result = set_subscr_events(sock, off, off, off, off);
			if (result < 0) {
				perror("Server setsockopt: SCTP_EVENTS");
				close(sock);
				exit(1);
			}

			peeloff_sk = sctp_peeloff(sock, assoc_id);
			if (peeloff_sk < 0) {
				perror("Server sctp_peeloff");
				close(sock);
				exit(1);
			}
			if (verbose) {
				printf("Server sctp_peeloff(3) on sk: %d with association ID: %d\n",
				       peeloff_sk, assoc_id);
				print_context(peeloff_sk, "Server PEELOFF");
			}

			/* Now get the client msg on peeloff socket */
			result = sctp_recvmsg(peeloff_sk, msglabel, sizeof(msglabel),
					      (struct sockaddr *)&sin, &sinlen,
					      NULL, &flags);
			if (result < 0) {
				perror("Server sctp_recvmsg-2");
				close(peeloff_sk);
				close(sock);
				exit(1);
			}

			if (verbose) {
				print_addr_info((struct sockaddr *)&sin,
						"Server SEQPACKET peeloff recvmsg");
				printf("peeloff association ID: %d\n",
				       assoc_id);
			}
		} else {
			printf("Invalid sctp_recvmsg response FLAGS: %x\n",
			       flags);
			close(peeloff_sk);
			close(sock);
			exit(1);
		}

		if (nopeer) {
			peerlabel = strdup("nopeer");
		} else if (snd_opt) {
			peerlabel = get_ip_option(sock, ipv4, &opt_len);

			if (!peerlabel)
				peerlabel = strdup("no_ip_options");
		} else {
			result = getpeercon(peeloff_sk, &peerlabel);
			if (result < 0) {
				perror("Server getpeercon");
				close(sock);
				close(peeloff_sk);
				exit(1);
			}
		}

		printf("Server PEELOFF %s: %s\n",
		       snd_opt ? "sock_opt" : "peer label", peerlabel);

		result = sctp_sendmsg(peeloff_sk, peerlabel,
				      strlen(peerlabel),
				      (struct sockaddr *)&sin,
				      sinlen, 0, 0, 0, 0, 0);
		if (result < 0) {
			perror("Server sctp_sendmsg");
			close(peeloff_sk);
			close(sock);
			exit(1);
		}

		if (verbose)
			printf("Server PEELOFF sent: %s\n", peerlabel);

		free(peerlabel);



		close(peeloff_sk);
	} while (1);

	close(sock);
	exit(0);
}

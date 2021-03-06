#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <err.h>
#include "../benchmarks.h"

#define BANDWIDTH_SIZE 4 MB
static const char pretty_name[] 
= "Time to send " __stringify(BANDWIDTH_SIZE) " between guests";
#define MB * 1024 * 1024

#define HIPQUAD(ip)				\
	((u8)(ip >> 24)),			\
	((u8)(ip >> 16)),			\
	((u8)(ip >> 8)),			\
	((u8)(ip))

static void send_data(int fd, const void *mem, unsigned long size)
{
	long ret, done = 0;
	while ((ret = write(fd, mem+done, size-done)) > 0) {
		done += ret;
		if (done == size)
			return;
	}
	if (ret < 0 || size != 0)
		err(1, "writing to other end");
}

static void receive_data(int fd, void *mem, unsigned long size)
{
	long ret, done = 0;
	while ((ret = read(fd, mem+done, size-done)) > 0) {
		done += ret;
		if (done == size)
			return;
	}
	if (ret < 0 || size != 0)
		err(1, "reading from other end");
}

static void do_bandwidth_bench(int fd, u32 runs,
			       struct benchmark *bench, const void *opts)
{
	/* We're going to send TCP packets to that addr. */
	struct sockaddr_in saddr;
	int sock;
	const struct pair_opt *opt = opts;
	char *mem = malloc(BANDWIDTH_SIZE);

	if (!mem)
		err(1, "allocating %i bytes", BANDWIDTH_SIZE);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		err(1, "creating socket");

	if (opt->start) {
		/* We accept connection from other client. */
		int listen_sock = sock;
		int set = 1;

		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(6100);
		saddr.sin_addr.s_addr = htonl(opt->yourip);
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) != 0)
			warn("setting SO_REUSEADDR");
		if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) != 0)
			err(1, "binding socket");

		if (listen(sock, 0) != 0)
			err(1, "listening on socket");

		send_ack(fd);

		sock = accept(listen_sock, NULL, 0);
		if (sock < 0)
			err(1, "accepting peer connection on socket");
		close(listen_sock);
	} else {
		/* We connect to other client. */
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(6100);
		saddr.sin_addr.s_addr = htonl(opt->otherip);
		if (connect(sock, (struct sockaddr *)&saddr, sizeof(saddr)))
			err(1, "connecting socket");

		send_ack(fd);
	}

	if (wait_for_start(fd)) {
		u32 i;
		/* Warmup first. */
		if (opt->start)
			send_data(sock, mem, NET_WARMUP_BYTES);
		else
			receive_data(sock, mem, NET_WARMUP_BYTES);

		for (i = 0; i < runs; i++) {
			if (opt->start)
				send_data(sock, mem, NET_BANDWIDTH_SIZE);
			else
				receive_data(sock, mem, NET_BANDWIDTH_SIZE);
		}
		send_ack(fd);
	}
	close(sock);
	free(mem);
}

static struct benchmark bandwidth_benchmark _benchmark_
= { "inter-tcp-bandwidth", pretty_name, do_pair_bench, do_bandwidth_bench };


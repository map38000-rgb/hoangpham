/*
 * netlog.c - user-space: nhan event tu ring buffer va in ra man hinh.
 *
 * Build (vi du, chinh lai duong dan cho NDK/toolchain cua ban):
 *   clang -g -O2 -target bpf -D__TARGET_ARCH_arm64 -I. -c netlog.bpf.c -o netlog.bpf.o
 *   bpftool gen skeleton netlog.bpf.o > netlog.skel.h
 *   $(CC) -g -O2 -I. netlog.c -lbpf -lelf -lz -o netlog
 *
 * Luu y: BPF_MAP_TYPE_RINGBUF can kernel >= 5.8.
 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <bpf/libbpf.h>
#include "netlog.h"
#include "netlog.skel.h"

#define AF_INET  2
#define AF_INET6 10

static volatile sig_atomic_t exiting;

static void on_signal(int sig)
{
	exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *fmt, va_list args)
{
	return vfprintf(stderr, fmt, args);
}

static int handle_event(void *ctx, void *data, size_t data_sz)
{
	const struct event *e = data;
	char src[INET6_ADDRSTRLEN] = "?";
	char dst[INET6_ADDRSTRLEN] = "?";
	const char *proto = "?";

	if (e->family == AF_INET) {
		proto = "IPv4";
		inet_ntop(AF_INET, &e->saddr_v4, src, sizeof(src));
		inet_ntop(AF_INET, &e->daddr_v4, dst, sizeof(dst));
	} else if (e->family == AF_INET6) {
		proto = "IPv6";
		inet_ntop(AF_INET6, &e->saddr_v6, src, sizeof(src));
		inet_ntop(AF_INET6, &e->daddr_v6, dst, sizeof(dst));
	}

	printf("%-16s pid=%-7u uid=%-7u pkg=%-24s %-4s %s:%u -> %s:%u\n",
	       e->comm, e->pid, e->uid, e->pkg_name, proto,
	       src, e->sport, dst, e->dport);

	return 0;
}

int main(void)
{
	struct netlog_bpf *skel;
	struct ring_buffer *rb = NULL;
	int err;

	libbpf_set_print(libbpf_print_fn);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	skel = netlog_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "Loi: khong mo/load duoc BPF skeleton\n");
		return 1;
	}

	err = netlog_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Loi: khong attach duoc kprobe (%d)\n", err);
		goto cleanup;
	}

	rb = ring_buffer__new(bpf_map__fd(skel->maps.events), handle_event, NULL, NULL);
	if (!rb) {
		err = -1;
		fprintf(stderr, "Loi: khong tao duoc ring buffer\n");
		goto cleanup;
	}

	printf("%-16s %-7s %-7s %-24s %-4s %s\n",
	       "COMM", "PID", "UID", "PKG", "PROTO", "SRC:PORT -> DST:PORT");

	while (!exiting) {
		err = ring_buffer__poll(rb, 200 /* ms */);
		if (err == -EINTR) {
			err = 0;
			break;
		}
		if (err < 0) {
			fprintf(stderr, "Loi khi poll ring buffer: %d\n", err);
			break;
		}
	}

cleanup:
	ring_buffer__free(rb);
	netlog_bpf__destroy(skel);
	return err < 0 ? 1 : 0;
}

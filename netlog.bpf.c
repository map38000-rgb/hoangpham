#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>
#include "netlog.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";

#define AF_INET  2
#define AF_INET6 10


struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} events SEC(".maps");


struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct event);
} heap SEC(".maps");

static __always_inline struct event *get_scratch_event(void)
{
	u32 zero = 0;

	return bpf_map_lookup_elem(&heap, &zero);
}


static __always_inline void read_pkg_name(struct event *ev, struct task_struct *task)
{
	struct mm_struct *mm = NULL;
	unsigned long arg_start = 0, arg_end = 0;
	long ret;

	BPF_CORE_READ_INTO(&mm, task, mm);
	if (!mm)
		goto fallback;

	BPF_CORE_READ_INTO(&arg_start, mm, arg_start);
	BPF_CORE_READ_INTO(&arg_end, mm, arg_end);

	if (!arg_start || arg_end <= arg_start)
		goto fallback;

	ret = bpf_probe_read_user_str(ev->pkg_name, sizeof(ev->pkg_name), (void *)arg_start);


	if (ret > 1)
		return;

fallback:
	__builtin_memset(ev->pkg_name, 0, sizeof(ev->pkg_name));
	bpf_get_current_comm(ev->pkg_name, sizeof(ev->comm));
}

SEC("kprobe/tcp_connect")
int BPF_KPROBE(bpf_prog_tcp_connect, struct sock *sk)
{
	struct event *ev, *rb_ev;
	struct task_struct *task;

	if (!sk)
		return 0;

	ev = get_scratch_event();
	if (!ev)
		return 0;

	__builtin_memset(ev, 0, sizeof(*ev));

	ev->pid = bpf_get_current_pid_tgid() >> 32;
	ev->uid = (u32)bpf_get_current_uid_gid();
	bpf_get_current_comm(ev->comm, sizeof(ev->comm));

	task = (struct task_struct *)bpf_get_current_task();
	read_pkg_name(ev, task);

	BPF_CORE_READ_INTO(&ev->family, sk, __sk_common.skc_family);
	BPF_CORE_READ_INTO(&ev->sport, sk, __sk_common.skc_num);
	BPF_CORE_READ_INTO(&ev->dport, sk, __sk_common.skc_dport);
	ev->dport = bpf_ntohs(ev->dport);

	if (ev->family == AF_INET) {
		BPF_CORE_READ_INTO(&ev->saddr_v4, sk, __sk_common.skc_rcv_saddr);
		BPF_CORE_READ_INTO(&ev->daddr_v4, sk, __sk_common.skc_daddr);
	} else if (ev->family == AF_INET6) {
		BPF_CORE_READ_INTO(&ev->saddr_v6, sk, __sk_common.skc_v6_rcv_saddr);
		BPF_CORE_READ_INTO(&ev->daddr_v6, sk, __sk_common.skc_v6_daddr);
	} else {
	
		return 0;
	}


	rb_ev = bpf_ringbuf_reserve(&events, sizeof(*ev), 0);
	if (!rb_ev) {
		bpf_printk("[NetLog] ringbuf full, drop pid=%d\n", ev->pid);
		return 0;
	}

	__builtin_memcpy(rb_ev, ev, sizeof(*ev));
	bpf_ringbuf_submit(rb_ev, 0);

	return 0;
}

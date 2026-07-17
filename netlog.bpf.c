#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 
 * Sử dụng Tracepoint của TCP thay cho Kprobe để tương thích với bpftool cũ trên Kernel 5.x
 */
SEC("tracepoint/tcp/tcp_probe")
int bpf_prog_tcp_probe(struct trace_event_raw_tcp_probe *ctx)
{
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    char comm[16];
    bpf_get_current_comm(&comm, sizeof(comm));

    /* Đọc địa chỉ IP trực tiếp từ cấu trúc của tracepoint tcp_probe */
    u16 sport = ctx->sport;
    u16 dport = ctx->dport;

    /* In log trực tiếp */
    bpf_printk("[NetLog] PID: %d | App: %s", pid, comm);
    bpf_printk("[NetLog] Port: %d -> %d", sport, dport);

    return 0;
}

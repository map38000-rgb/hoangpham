
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* Hook vào hàm tcp_connect của kernel */
SEC("kprobe/tcp_connect")
int BPF_KPROBE_PROTOTYPE(tcp_connect, struct pt_regs *ctx)
{
    /* Lấy tham số đầu tiên của hàm tcp_connect (chính là struct sock *sk) */
    struct sock *sk = (struct sock *)PT_REGS_PARM1_CORE(ctx);
    if (!sk)
        return 0;

    u32 pid;
    char comm[16];
    u32 saddr = 0, daddr = 0;

    /* Lấy PID (32-bit cao của bpf_get_current_pid_tgid) */
    pid = bpf_get_current_pid_tgid() >> 32;

    /* Lấy tên tiến trình (package/command name) */
    bpf_get_current_comm(&comm, sizeof(comm));

    /* Đọc an toàn địa chỉ IP nguồn và đích bằng CO-RE */
    BPF_CORE_READ_INTO(&saddr, sk, __sk_common.skc_rcv_saddr);
    BPF_CORE_READ_INTO(&daddr, sk, __sk_common.skc_daddr);

    /* In log ra trace_pipe */
    bpf_printk("[NetLog] PID: %d | App: %s", pid, comm);
    bpf_printk("[NetLog] Src: %pI4 -> Dst: %pI4", &saddr, &daddr);

    return 0;
}

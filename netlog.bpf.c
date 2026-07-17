#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 
 * Tự định nghĩa cách lấy tham số đầu tiên (RDI trên x86_64, regs[0] trên ARM64)
 * Tránh sử dụng bpf_tracing.h của hệ thống để không bị xung đột struct pt_regs
 */
static __always_inline unsigned long get_first_param(struct pt_regs *ctx) {
#if defined(__TARGET_ARCH_x86)
    // Trên x86_64, tham số 1 nằm ở thanh ghi di (hoặc rdi)
    return ctx->di;
#elif defined(__TARGET_ARCH_arm64)
    // Trên arm64, tham số 1 nằm ở regs[0]
    return ctx->regs[0];
#else
    return 0;
#endif
}

SEC("kprobe/tcp_connect")
int BPF_KPROBE_PROTOTYPE_FIX(struct pt_regs *ctx)
{
    // Lấy con trỏ struct sock *sk từ tham số đầu tiên của hàm tcp_connect
    struct sock *sk = (struct sock *)get_first_param(ctx);
    if (!sk)
        return 0;

    u32 pid;
    char comm[16];
    u32 saddr = 0, daddr = 0;

    /* Lấy PID (32-bit cao) */
    pid = bpf_get_current_pid_tgid() >> 32;

    /* Lấy tên tiến trình */
    bpf_get_current_comm(&comm, sizeof(comm));

    /* Đọc an toàn địa chỉ IP bằng CO-RE */
    BPF_CORE_READ_INTO(&saddr, sk, __sk_common.skc_rcv_saddr);
    BPF_CORE_READ_INTO(&daddr, sk, __sk_common.skc_daddr);

    /* In log ra trace_pipe */
    bpf_printk("[NetLog] PID: %d | App: %s", pid, comm);
    bpf_printk("[NetLog] Src: %pI4 -> Dst: %pI4", &saddr, &daddr);

    return 0;
}

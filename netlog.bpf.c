#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 
 * Sử dụng fentry với cú pháp nguyên thủy (raw context).
 * Không cần include bpf_tracing.h, loại bỏ hoàn toàn lỗi macro.
 */
SEC("fentry/tcp_connect")
int bpf_prog_tcp_connect(__u64 *ctx)
{
    /* Tham số thứ nhất của hàm tcp_connect(struct sock *sk) chính là ctx[0] */
    struct sock *sk = (struct sock *)ctx[0];
    if (!sk)
        return 0;

    u32 pid;
    char comm[16];
    u32 saddr = 0, daddr = 0;

    /* Lấy PID (32-bit cao của bpf_get_current_pid_tgid) */
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

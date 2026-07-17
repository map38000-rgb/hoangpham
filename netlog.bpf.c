#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 
 * Kprobe tương thích tối đa cho kernel Android ARM64
 */
SEC("kprobe/tcp_connect")
int bpf_prog_tcp_connect(void *ctx)
{
    /* 
     * [BÍ QUYẾT ARM64] 
     * Trên ARM64, struct pt_regs bắt đầu bằng mảng thanh ghi (u64 regs[31]).
     * Thanh ghi X0 (tham số thứ nhất) nằm ở vị trí bộ nhớ đầu tiên.
     * Ép kiểu ctx về con trỏ (u64 *) và lấy phần tử [0] để lấy X0 (chính là struct sock *).
     */
    struct sock *sk = (struct sock *)(((u64 *)ctx)[0]);
    if (!sk)
        return 0;

    u32 pid;
    char comm[16];
    u32 saddr = 0, daddr = 0;

    /* Lấy PID (tiến trình Android) */
    pid = bpf_get_current_pid_tgid() >> 32;

    /* Lấy tên Package/App */
    bpf_get_current_comm(&comm, sizeof(comm));

    /* Sử dụng CO-RE để đọc dữ liệu an toàn trên các phiên bản Kernel Android khác nhau */
    BPF_CORE_READ_INTO(&saddr, sk, __sk_common.skc_rcv_saddr);
    BPF_CORE_READ_INTO(&daddr, sk, __sk_common.skc_daddr);

    /* In log ra trace_pipe của Android */
    bpf_printk("[NetLog] PID: %d | App: %s", pid, comm);
    bpf_printk("[NetLog] Src: %pI4 -> Dst: %pI4", &saddr, &daddr);

    return 0;
}

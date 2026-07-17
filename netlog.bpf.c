#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h> // Bắt buộc thêm để dùng BPF_KPROBE

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 
 * Kprobe kernel Android ARM64
 */
SEC("kprobe/tcp_connect")
int BPF_KPROBE(bpf_prog_tcp_connect, struct sock *sk)
{
    // BPF_KPROBE tự động cast ctx và lấy tham số `sk` an toàn cho bạn.
    if (!sk)
        return 0;

    u32 pid;
    char comm[16];
    u32 saddr = 0, daddr = 0;

    // Lấy PID (tgid)
    pid = bpf_get_current_pid_tgid() >> 32;

    // Lấy tên tiến trình (Command name)
    bpf_get_current_comm(&comm, sizeof(comm));

    // Dùng CO-RE để đọc dữ liệu từ struct sock
    BPF_CORE_READ_INTO(&saddr, sk, __sk_common.skc_rcv_saddr);
    BPF_CORE_READ_INTO(&daddr, sk, __sk_common.skc_daddr);
    
    // In log. Lưu ý saddr và daddr vốn đã ở dạng Network Byte Order (Big Endian) 
    // nên dùng %pI4 kèm theo reference (pointer) là chính xác.
    bpf_printk("[NetLog] PID: %d | App: %s\n", pid, comm);
    bpf_printk("[NetLog] Src: %pI4 -> Dst: %pI4\n", &saddr, &daddr);

    return 0;
}

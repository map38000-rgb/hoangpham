#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* Hook vào hàm tcp_connect của kernel để bắt các kết nối TCP IPv4 gửi đi */
SEC("kprobe/tcp_connect")
int BPF_KPROBE(tcp_connect, struct sock *sk)
{
    u32 pid;
    char comm[16];
    u32 saddr, daddr;
    u16 dport;

    /* Lấy PID (32-bit cao của bpf_get_current_pid_tgid) */
    pid = bpf_get_current_pid_tgid() >> 32;

    /* Lấy tên tiến trình (package/command name) */
    bpf_get_current_comm(&comm, sizeof(comm));

    /* Sử dụng BPF CO-RE (Compile Once - Run Everywhere) để đọc dữ liệu từ struct sock */
    BPF_CORE_READ_INTO(&saddr, sk, __sk_common.skc_rcv_saddr);
    BPF_CORE_READ_INTO(&daddr, sk, __sk_common.skc_daddr);
    BPF_CORE_READ_INTO(&dport, sk, __sk_common.skc_dport);

    /* 
     * In log ra trace_pipe. 
     * Kernel hiện đại hỗ trợ format %pI4 để in IP IPv4 từ con trỏ.
     * Lưu ý: bpf_printk bị giới hạn số lượng tham số, nên ta chia làm 2 dòng.
     */
    bpf_printk("[NetLog] PID: %d | App: %s", pid, comm);
    bpf_printk("[NetLog] Src IP: %pI4 -> Dst IP: %pI4", &saddr, &daddr);

    return 0;
}

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h> 

char LICENSE[] SEC("license") = "Dual BSD/GPL";

/* 
 * Kprobe kernel Android ARM64
 */
SEC("kprobe/tcp_connect")
int BPF_KPROBE(bpf_prog_tcp_connect, struct sock *sk)
{
    
    if (!sk)
        return 0;

    u32 pid;
    char comm[16];
    u32 saddr = 0, daddr = 0;

    pid = bpf_get_current_pid_tgid() >> 32;

    bpf_get_current_comm(&comm, sizeof(comm));

    BPF_CORE_READ_INTO(&saddr, sk, __sk_common.skc_rcv_saddr);
    BPF_CORE_READ_INTO(&daddr, sk, __sk_common.skc_daddr);
    

    bpf_printk("[NetLog] PID: %d | App: %s\n", pid, comm);
    bpf_printk("[NetLog] Src: %pI4 -> Dst: %pI4\n", &saddr, &daddr);

    return 0;
}

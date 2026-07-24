#ifndef __NETLOG_H
#define __NETLOG_H

#define TASK_COMM_LEN 16
#define PKG_NAME_LEN  128

/* Dung chung giua BPF program va user-space: giu dung kich thuoc tung field
 * de tranh lech struct layout giua 2 ben khi build bang compiler khac nhau. */
struct event {
	__u32 pid;
	__u32 uid;
	__u16 family;   /* AF_INET (2) hoac AF_INET6 (10) */
	__u16 sport;
	__u16 dport;
	__u16 pad;      /* khong dung, giu alignment cho union ben duoi */
	union {
		__u32 saddr_v4;
		__u8  saddr_v6[16];
	};
	union {
		__u32 daddr_v4;
		__u8  daddr_v6[16];
	};
	char comm[TASK_COMM_LEN];
	char pkg_name[PKG_NAME_LEN];
};

#endif /* __NETLOG_H */

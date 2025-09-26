// injector_arm64.c
// Purpose: attach to a PID, try to locate remote dlopen address and print it.
// Build (example):
// $NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang -O2 -fPIE -pie -o injector_arm64 injector_arm64.c -ldl

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <libgen.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

// Provide a simple aarch64 user_pt_regs definition if not present
#if defined(__aarch64__)
/* Minimal user_pt_regs for aarch64 to work with PTRACE_GETREGSET.
   This matches common kernel layout for regs used here; it is enough for reading/writing. */
struct user_pt_regs {
    unsigned long regs[31];
    unsigned long sp;
    unsigned long pc;
    unsigned long pstate;
};
typedef struct user_pt_regs REG_TYPE;
#else
#error "This file targets aarch64 only"
#endif

static void perror_exit(const char *msg){
    perror(msg);
    exit(1);
}

static pid_t parse_pid(const char *s){
    return (pid_t)atoi(s);
}

static int ptrace_attach(pid_t pid){
    if(ptrace(PTRACE_ATTACH, pid, NULL, NULL) != 0) return -1;
    int status;
    waitpid(pid, &status, WUNTRACED);
    return 0;
}

static int ptrace_detach(pid_t pid){
    if(ptrace(PTRACE_DETACH, pid, NULL, NULL) != 0) return -1;
    return 0;
}

static int ptrace_getregs(pid_t pid, REG_TYPE *regs){
    struct iovec iov;
    iov.iov_base = regs;
    iov.iov_len = sizeof(REG_TYPE);
    if(ptrace(PTRACE_GETREGSET, pid, (void*)NT_PRSTATUS, &iov) == -1){
        return -1;
    }
    return 0;
}

static int ptrace_setregs(pid_t pid, REG_TYPE *regs){
    struct iovec iov;
    iov.iov_base = regs;
    iov.iov_len = sizeof(REG_TYPE);
    if(ptrace(PTRACE_SETREGSET, pid, (void*)NT_PRSTATUS, &iov) == -1){
        return -1;
    }
    return 0;
}

static int ptrace_write(pid_t pid, void *dest, const void *src, size_t len){
    size_t i = 0;
    long val;
    const uint8_t *s = src;
    uint8_t *d = dest;
    for (; i + 8 <= len; i += 8) {
        memcpy(&val, s + i, 8);
        if (ptrace(PTRACE_POKEDATA, pid, d + i, val) == -1) return -1;
    }
    if (i < len) {
        uint8_t buf[8] = {0};
        memcpy(buf, s + i, len - i);
        memcpy(&val, buf, 8);
        if (ptrace(PTRACE_POKEDATA, pid, d + i, val) == -1) return -1;
    }
    return 0;
}

// Robust remote symbol finder by basename (module_basename: "libc.so" or "linker64")
static void *get_remote_symbol_address_by_basename(pid_t pid, const char* module_basename, const char* symbol){
    char maps_path[64], line[512];
    unsigned long remote_base = 0;
    FILE *f = NULL;

    // 1) find remote base by basename
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    f = fopen(maps_path, "r");
    if(!f) return NULL;
    while(fgets(line, sizeof(line), f)){
        char *p = strrchr(line, '/');
        if(p){
            char *bn = basename(p);
            if(bn && strcmp(bn, module_basename) == 0){
                unsigned long a,b;
                if(sscanf(line, "%lx-%lx", &a, &b) == 2){
                    remote_base = a;
                    break;
                }
            }
        } else {
            if(strstr(line, module_basename)){
                unsigned long a,b;
                if(sscanf(line, "%lx-%lx", &a, &b) == 2){
                    remote_base = a;
                    break;
                }
            }
        }
    }
    fclose(f);
    if(!remote_base) return NULL;

    // 2) find local base for module_basename from /proc/self/maps
    unsigned long local_base = 0;
    FILE *fs = fopen("/proc/self/maps","r");
    if(!fs) return NULL;
    while(fgets(line, sizeof(line), fs)){
        char *p = strrchr(line, '/');
        if(p){
            char *bn = basename(p);
            if(bn && strcmp(bn, module_basename) == 0){
                unsigned long a,b;
                if(sscanf(line, "%lx-%lx", &a, &b) == 2){
                    local_base = a;
                    break;
                }
            }
        } else {
            if(strstr(line, module_basename)){
                unsigned long a,b;
                if(sscanf(line, "%lx-%lx", &a, &b) == 2){
                    local_base = a;
                    break;
                }
            }
        }
    }
    fclose(fs);
    if(!local_base) return NULL;

    // 3) get local symbol address: try process-wide dlsym
    void *local_sym = dlsym(RTLD_DEFAULT, symbol);
    if(!local_sym){
        // fallback: try dlopen of common full paths with RTLD_NOW|RTLD_NOLOAD
        const char *cands[] = {
            "/system/bin/linker64",
            "/apex/com.android.runtime/bin/linker64",
            "/system/lib64/libc.so",
            "/apex/com.android.runtime/lib64/bionic/libc.so",
            NULL
        };
        for(int i=0; cands[i]; ++i){
            void *h = dlopen(cands[i], RTLD_NOW | RTLD_NOLOAD);
            if(h){
                local_sym = dlsym(h, symbol);
                dlclose(h);
                if(local_sym) break;
            }
        }
    }
    if(!local_sym) return NULL;

    unsigned long offset = (unsigned long)local_sym - local_base;
    void *remote_addr = (void*)(remote_base + offset);
    return remote_addr;
}

static void usage(const char *p){
    fprintf(stderr, "Usage: %s <pid> <lib_full_path_to_inject>\n", p);
    fprintf(stderr, "This tool currently ATTACHES and prints candidate remote dlopen addresses.\n");
    fprintf(stderr, "It does not perform the final remote dlopen call (safe-by-default).\n");
}

int main(int argc, char **argv){
    if(argc < 3){
        usage(argv[0]);
        return 1;
    }
    pid_t pid = parse_pid(argv[1]);
    const char *lib_path = argv[2];

    // quick checks
    if(access(lib_path, R_OK) != 0){
        fprintf(stderr, "Library not readable: %s (%s)\n", lib_path, strerror(errno));
        // continue: user may still want to get dlopen address
    }

    printf("[*] Attaching to pid %d ...\n", pid);
    if(ptrace_attach(pid) != 0){
        fprintf(stderr, "ptrace_attach failed: %s\n", strerror(errno));
        return 1;
    }
    printf("[+] attached.\n");

    // Try various candidate basenames and symbols
    const char *candidates[] = {"linker64", "libc.so", "libdl.so", "linker", NULL};
    const char *symbols[] = {"dlopen", "android_dlopen_ext", NULL};

    for(int i=0; candidates[i]; ++i){
        for(int j=0; symbols[j]; ++j){
            void *addr = get_remote_symbol_address_by_basename(pid, candidates[i], symbols[j]);
            if(addr){
                printf("[+] Found %s in %s -> remote address: %p (symbol %s)\n",
                       symbols[j], candidates[i], addr, symbols[j]);
            } else {
                printf("[-] %s not found in %s\n", symbols[j], candidates[i]);
            }
        }
    }

    printf("[*] Note: this tool currently does NOT perform remote dlopen.\n");
    printf("[*] If you want full injection (mmap remote, write path, call dlopen), tell me and I will generate the full injector routine (it is riskier and device-specific).\n");

    if(ptrace_detach(pid) != 0){
        fprintf(stderr, "ptrace_detach failed: %s\n", strerror(errno));
        return 1;
    }
    printf("[+] detached.\n");
    return 0;
}

// injector_arm64.c
// Build (example with Android NDK clang):
// aarch64-linux-android21-clang -O2 -static -o injector_arm64 injector_arm64.c

#define _GNU_SOURCE
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <dirent.h>
#include <sys/personality.h>
#include <linux/limits.h>
#include <sys/user.h>
#include <syscall.h>
#include <libgen.h>
#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif
typedef unsigned long ulong;
typedef long long sll;

#if defined(__aarch64__)
#define REG_TYPE struct user_pt_regs
#else
#error "This injector is for aarch64 only"
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
    // PTRACE_GETREGSET returns 0 on success
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
    // write word by word (8 bytes)
    for (; i + 8 <= len; i += 8) {
        memcpy(&val, s + i, 8);
        if (ptrace(PTRACE_POKEDATA, pid, d + i, val) == -1) return -1;
    }
    // leftover
    if (i < len) {
        uint8_t buf[8] = {0};
        memcpy(buf, s + i, len - i);
        memcpy(&val, buf, 8);
        if (ptrace(PTRACE_POKEDATA, pid, d + i, val) == -1) return -1;
    }
    return 0;
}

static void* get_remote_symbol_address(pid_t pid, const char* module, const char* symbol){
    // Parse /proc/<pid>/maps to find remote module base, then compute symbol offset from local.
    FILE *f;
    char maps_path[64], line[512];
    unsigned long base = 0;
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    f = fopen(maps_path, "r");
    if(!f) return NULL;
    while(fgets(line, sizeof(line), f)){
        if(strstr(line, module)){
            unsigned long a,b_local;
            if(sscanf(line, "%lx-%lx", &a,&b_local)==2){
                base = a;
                break;
            }
        }
    }
    fclose(f);
    if(!base) return NULL;

    // load local module handle
    void *local_handle = dlopen(module, RTLD_NOW);
    if(!local_handle) return NULL;
    void *local_sym = dlsym(local_handle, symbol);
    if(!local_sym){
        dlclose(local_handle);
        return NULL;
    }
    // find local module base by reading /proc/self/maps
    FILE *fs = fopen("/proc/self/maps", "r");
    if(!fs){
        dlclose(local_handle);
        return NULL;
    }
    unsigned long local_base = 0;
    unsigned long b = 0;
    while(fgets(line,sizeof(line),fs)){
        if(strstr(line, module)){
            if(sscanf(line, "%lx-%lx", &local_base, &b)==2) break;
        }
    }
    fclose(fs);
    if(!local_base){
        dlclose(local_handle);
        return NULL;
    }
    unsigned long offset = (unsigned long)local_sym - local_base;
    void *remote_addr = (void*)(base + offset);
    dlclose(local_handle);
    return remote_addr;
}

static void *get_remote_symbol_address_by_basename(pid_t pid, const char* module_basename, const char* symbol){
    char maps_path[64], line[512];
    unsigned long remote_base = 0;
    FILE *f = NULL;

    // 1) find remote base by basename
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);
    f = fopen(maps_path, "r");
    if(!f) return NULL;
    while(fgets(line, sizeof(line), f)){
        // the module path is at end of line; check if line contains module_basename
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
            // sometimes maps line might contain just module_basename without full path
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

    // 3) get local symbol address: try dlsym on RTLD_DEFAULT (process-wide). If symbol is in linker/libc, dlsym should find.
    void *local_sym = dlsym(RTLD_DEFAULT, symbol);
    if(!local_sym){
        // fallback: try dlopen of common full paths to get symbol
        const char *cands[] = {"/system/lib64/libc.so","/apex/com.android.runtime/lib64/bionic/libc.so","/system/bin/linker64","/apex/com.android.runtime/bin/linker64", NULL};
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/mman.h>

// Dobby header
#include "dobby.h"   // đường dẫn tới dobby.h (điều chỉnh include path khi build)

#define TAG "CAMERA_DOBBY"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// typedef cho orig function (khả năng: trả về pointer, không có param)
typedef void* (*get_main_t)(void);

// con trỏ tới trampoline do Dobby cấp
static get_main_t orig_get_main = NULL;

// đọc base address của module
static uintptr_t get_module_base(const char *module_name) {
    FILE *fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;
    char line[512];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, module_name)) {
            // ví dụ: 7f000000-7f200000 r-xp ...
            sscanf(line, "%lx-%*lx %*s %*s %*s %*d", &base);
            break;
        }
    }
    fclose(fp);
    return base;
}

// hook implementation (gọi orig_get_main và in log)
void* my_get_main(void) {
    void* cam = NULL;
    if (orig_get_main) {
        // gọi hàm gốc thông qua trampoline do Dobby tạo
        cam = orig_get_main();
    } else {
        LOGE("orig_get_main == NULL");
    }
    LOGI("Camera::get_main() -> %p", cam);
    return cam;
}

// Thread chờ lib + cài hook
void *wait_and_hook(void *arg) {
    const uintptr_t RVA_get_main = 0x7e6c098;      // RVA bạn cung cấp
    const char *target_lib = "libil2cpp.so";      // hoặc "GameAssembly.so" nếu phù hợp
    uintptr_t base = 0;

    LOGI("Waiting for %s ...", target_lib);

    // chờ tối đa ~30s (300 * 100ms)
    for (int i = 0; i < 300; ++i) {
        base = get_module_base(target_lib);
        if (base) break;
        usleep(100000); // 100ms
    }

    if (!base) {
        LOGE("Cannot find module %s after waiting", target_lib);
        return NULL;
    }

    uintptr_t func_addr = base + RVA_get_main;
    LOGI("%s base: 0x%lx", target_lib, base);
    LOGI("Camera::get_main() addr: 0x%lx", func_addr);

    // DobbyHook: target, replacement, &backup
    int ret = DobbyHook((void*)func_addr, (void*)my_get_main, (void**)&orig_get_main);
    if (ret == 0) {
        LOGI("DobbyHook success: Camera::get_main hooked");
    } else {
        LOGE("DobbyHook failed: ret=%d", ret);
    }

    return NULL;
}

__attribute__((constructor))
void on_load() {
    pthread_t t;
    if (pthread_create(&t, NULL, wait_and_hook, NULL) == 0) {
        pthread_detach(t);
    } else {
        LOGE("Failed to create wait_and_hook thread");
    }
}

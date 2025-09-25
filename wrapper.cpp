#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>

#define LOG_TAG "INLINEHOOK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Hàm bạn muốn hook
typedef void (*original_func_t)();
original_func_t original_func = nullptr;

// Hàm thay thế
void my_hooked_func() {
    LOGI("my_hooked_func called!");
    if (original_func) {
        // gọi hàm gốc nếu muốn
        original_func();
    }
}

// Patch inline
void inline_hook(void* target, void* hook) {
    // Change memory protection
    uintptr_t page_start = (uintptr_t)target & ~(getpagesize() - 1);
    if (mprotect((void*)page_start, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        LOGE("mprotect failed");
        return;
    }

    // x86_64 / arm64: mình demo ARM64 B branch: BL hook
    // Cách đơn giản nhất: override first 4 bytes (demo, cần tùy hàm)
    unsigned char* p = (unsigned char*)target;
    // save original 4 bytes
    static unsigned char backup[16];
    memcpy(backup, p, 16);

    // patch: ldr x16, #0; br x16 (simple jump)
    // actual patch sẽ cần disasm/asm chính xác
    // ở đây demo placeholder
    // memcpy(p, patch_bytes, patch_size);

    __builtin___clear_cache((char*)target, (char*)target + 16);
}

// Tìm hàm theo offset trong lib đã load
void* find_func_offset(void* handle, size_t offset) {
    return (char*)handle + offset;
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("wrapper inline-hook entered");

    void* handle = dlopen("libmain_real.so", RTLD_NOW);
    if (!handle) {
        LOGE("dlopen libmain_real.so failed: %s", dlerror());
        return JNI_VERSION_1_6;
    }

    // Giả sử bạn đã reverse engineer offset của hàm muốn hook
    size_t target_offset = 0x1234; // ví dụ
    void* target_func = find_func_offset(handle, target_offset);
    original_func = (original_func_t)target_func;

    // patch inline
    inline_hook(target_func, (void*)&my_hooked_func);

    return JNI_VERSION_1_6;
}

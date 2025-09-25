// wrapper.cpp
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <cstdio>
#include <cstring>

#define LOG_TAG "WRAPPER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef jint (*JNI_OnLoad_t)(JavaVM*, void*);

// Helper: try RTLD_NEXT first, then explicit dlopen candidates
static void* find_original_handle() {
    // If original already loaded and available via RTLD_NEXT symbols, we might not need explicit handle.
    // But some linkers / namespaces require explicit dlopen.
    const char* candidates[] = {
        "libmain_real.so",   // preferred: what we rename original to in APK
        "libunity.so",
        "libil2cpp.so",
        "libmono.so",
        nullptr
    };

    for (const char** p = candidates; *p; ++p) {
        void* h = dlopen(*p, RTLD_NOW | RTLD_GLOBAL);
        if (h) {
            LOGI("dlopen succeeded: %s -> %p", *p, h);
            return h;
        } else {
            // debug
            // LOGI("dlopen failed for %s: %s", *p, dlerror());
        }
    }
    return nullptr;
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("Wrapper JNI_OnLoad called");

    // 1) Try to get the next symbol via RTLD_NEXT
    void* sym = dlsym(RTLD_NEXT, "JNI_OnLoad");
    if (sym) {
        LOGI("Found JNI_OnLoad via RTLD_NEXT: %p", sym);
        JNI_OnLoad_t next_onload = (JNI_OnLoad_t)sym;

        // --- place your pre-init hook here ---
        LOGI("Running pre-init hooks (RTLD_NEXT path)");

        jint res = next_onload(vm, reserved);

        // --- place your post-init hook here ---
        LOGI("Running post-init hooks (RTLD_NEXT path), returned %d", res);
        return res;
    }

    // 2) Fallback: explicit dlopen of original lib (renamed in APK)
    void* handle = find_original_handle();
    if (!handle) {
        LOGE("Cannot find original Unity lib (dlopen failed for all candidates).");
        return -1;
    }

    // 3) find JNI_OnLoad in original
    JNI_OnLoad_t orig_onload = (JNI_OnLoad_t)dlsym(handle, "JNI_OnLoad");
    if (!orig_onload) {
        LOGE("dlsym for JNI_OnLoad failed: %s", dlerror());
        // Not necessarily fatal: maybe original library doesn't export JNI_OnLoad; but most Unity libs do.
        return -1;
    }

    LOGI("Calling original JNI_OnLoad at %p", orig_onload);

    // --- place your pre-init hook here ---
    LOGI("Running pre-init hooks (dlopen path)");

    jint result = orig_onload(vm, reserved);

    // --- place your post-init hook here ---
    LOGI("Running post-init hooks (dlopen path), returned %d", result);

    return result;
}

// Provide an optional constructor to do early initialization if needed
__attribute__((constructor)) static void wrapper_ctor() {
    LOGI("Wrapper constructor executed");
    // You can initialize hooking framework, open logs, etc.
}

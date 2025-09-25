// wrapper.cpp
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "WRAPPER", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "WRAPPER", __VA_ARGS__)

typedef jint (*JNI_OnLoad_t)(JavaVM*, void*);

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("Fake libmain loaded, trying RTLD_NEXT...");
    JNI_OnLoad_t next_onload = (JNI_OnLoad_t)dlsym(RTLD_NEXT, "JNI_OnLoad");
    if (next_onload) {
        LOGI("Found next JNI_OnLoad via RTLD_NEXT, calling it.");
        // place hooks/init here
        return next_onload(vm, reserved);
    }

    const char* candidates[] = {"libunity.so", "libil2cpp.so", "libmain_real.so", nullptr};
    void* handle = nullptr;
    for (const char** p = candidates; *p; ++p) {
        handle = dlopen(*p, RTLD_NOW);
        if (handle) {
            LOGI("dlopen succeeded: %s", *p);
            break;
        }
    }
    if (!handle) {
        LOGE("Unable to find original Unity lib (dlopen failed).");
        return -1;
    }

    JNI_OnLoad_t orig = (JNI_OnLoad_t)dlsym(handle, "JNI_OnLoad");
    if (!orig) {
        LOGE("dlsym JNI_OnLoad failed: %s", dlerror());
        return -1;
    }

    // place hooks/init here

    return orig(vm, reserved);
}

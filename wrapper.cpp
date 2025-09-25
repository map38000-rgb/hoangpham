// wrapper_smart.cpp
#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

#define LOG_TAG "WRAPPER"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef jint (*JNI_OnLoad_t)(JavaVM*, void*);

static void log_maps() {
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        LOGI("MAP: %s", line.c_str());
    }
}

// Try RTLD_NEXT first
static JNI_OnLoad_t try_rtld_next() {
    void* sym = dlsym(RTLD_NEXT, "JNI_OnLoad");
    if (sym) {
        LOGI("Found JNI_OnLoad via RTLD_NEXT: %p", sym);
        return (JNI_OnLoad_t)sym;
    }
    LOGI("RTLD_NEXT: no JNI_OnLoad");
    return nullptr;
}

// Iterate /proc/self/maps, try dlopen(path, RTLD_NOLOAD) and dlsym
static JNI_OnLoad_t search_maps_for_onload() {
    std::ifstream maps("/proc/self/maps");
    if (!maps.is_open()) {
        LOGE("Cannot open /proc/self/maps");
        return nullptr;
    }
    std::string line;
    std::vector<std::string> seen;
    while (std::getline(maps, line)) {
        // line format: addr perms offset dev inode pathname
        auto pos = line.find('/');
        if (pos == std::string::npos) continue;
        std::string path = line.substr(pos);
        if (path.empty()) continue;
        // filter duplicates
        bool dup = false;
        for (auto &s: seen) if (s == path) { dup = true; break; }
        if (dup) continue;
        seen.push_back(path);

        // only .so files (quick filter)
        if (path.find(".so") == std::string::npos) continue;

        // Try to get handle without loading: RTLD_NOLOAD
        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
        if (!handle) {
            // maybe it's loaded under a different namespace; try normal dlopen (may increase risk)
            // but skip heavy dlopen by default
            // LOGI("dlopen NOLOAD failed for %s: %s", path.c_str(), dlerror());
            continue;
        }
        LOGI("dlopen(NOLOAD) succeeded for %s -> handle=%p", path.c_str(), handle);
        void* sym = dlsym(handle, "JNI_OnLoad");
        if (sym) {
            LOGI("Found JNI_OnLoad in %s -> %p", path.c_str(), sym);
            return (JNI_OnLoad_t)sym;
        } else {
            //LOGI("No JNI_OnLoad in %s", path.c_str());
        }
        // do not dlclose handles returned by dlopen(RTLD_NOLOAD)
    }
    LOGI("search_maps_for_onload: not found");
    return nullptr;
}

// Try explicit names (fallback)
static JNI_OnLoad_t try_explicit_candidates() {
    const char* candidates[] = {
        "libmain_real.so",
        "libmain.so", // in case original still there
        "libunity.so",
        "libil2cpp.so",
        "libmono.so",
        nullptr
    };

    for (const char** p = candidates; *p; ++p) {
        void* handle = dlopen(*p, RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
            LOGI("dlopen failed for %s: %s", *p, dlerror());
            continue;
        }
        LOGI("dlopen succeeded for %s -> %p", *p, handle);
        void* sym = dlsym(handle, "JNI_OnLoad");
        if (sym) {
            LOGI("Found JNI_OnLoad in candidate %s -> %p", *p, sym);
            return (JNI_OnLoad_t)sym;
        }
    }
    LOGI("try_explicit_candidates: not found");
    return nullptr;
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("=== wrapper_smart JNI_OnLoad entered ===");
    log_maps();

    // 1) RTLD_NEXT
    if (JNI_OnLoad_t func = try_rtld_next()) {
        LOGI("Calling RTLD_NEXT JNI_OnLoad");
        // pre-init hook area
        jint r = func(vm, reserved);
        // post-init hook area
        LOGI("RTLD_NEXT JNI_OnLoad returned %d", r);
        return r;
    }

    // 2) Search loaded modules via /proc/self/maps
    if (JNI_OnLoad_t func = search_maps_for_onload()) {
        LOGI("Calling JNI_OnLoad found in maps");
        jint r = func(vm, reserved);
        LOGI("maps-found JNI_OnLoad returned %d", r);
        return r;
    }

    // 3) Try explicit candidate names
    if (JNI_OnLoad_t func = try_explicit_candidates()) {
        LOGI("Calling JNI_OnLoad from explicit candidate");
        jint r = func(vm, reserved);
        LOGI("explicit candidate JNI_OnLoad returned %d", r);
        return r;
    }

    LOGE("Failed to find any JNI_OnLoad to forward to. Crash likely.");
    return -1;
}

__attribute__((constructor)) static void wrapper_ctor() {
    LOGI("wrapper_smart constructor running");
}

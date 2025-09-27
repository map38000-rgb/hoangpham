// mylib.c
#include <stdio.h>
#include <time.h>
#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>   // cần cho errno

#define LOG_TAG "MYLIB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// try write to sdcard first, fallback to /data/local/tmp
static void write_log_file(const char *msg) {
    const char *paths[] = {
        "/sdcard/mylib_log.txt",
        "/sdcard/Download/mylib_log.txt",
        "/data/local/tmp/mylib_log.txt",
        NULL
    };

    for (int i = 0; paths[i] != NULL; ++i) {
        const char *p = paths[i];

        if (i == 1) {
            mkdir("/sdcard/Download", 0777);
        }

        FILE *f = fopen(p, "a");
        if (!f) {
            LOGI("cannot open %s for append (errno=%d), try next", p, errno);
            continue;
        }

        time_t t = time(NULL);
        struct tm tm;
        if (localtime_r(&t, &tm)) {
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
            fprintf(f, "[%s] %s\n", buf, msg);
        } else {
            fprintf(f, "[ts?] %s\n", msg);
        }
        fclose(f);
        LOGI("wrote log to %s", p);
        return;
    }

    LOGE("failed to write log to any path");
}

__attribute__((constructor))
static void on_load(void) {
    const char *msg = "mylib loaded successfully";
    LOGI("%s", msg);
    write_log_file(msg);
}

#include <jni.h>
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnLoad called");
    write_log_file("JNI_OnLoad called");
    return JNI_VERSION_1_6;
}

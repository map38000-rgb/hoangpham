#include <stdio.h>
#include <time.h>
#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

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
            // ignore error, maybe already exists
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

/* --- worker thread --- */
static void *init_worker(void *arg) {
    (void)arg;
    LOGI("init_worker: started");
    write_log_file("mylib init_worker started");
    // thêm các bước init nặng khác ở đây, nếu cần, nhưng tránh gọi API Java đồng bộ
    write_log_file("mylib init_worker done");
    LOGI("init_worker: done");
    return NULL;
}

/* Nếu bạn muốn giữ constructor, chỉ để log rất nhẹ (không I/O) hoặc xóa hẳn */
__attribute__((constructor))
static void on_load(void) {
    // chỉ log tới logcat (không file I/O)
    LOGI("mylib constructor called (no heavy work here)");
}

/* JNI_OnLoad: trả về nhanh và spawn detached worker thread */
#include <jni.h>
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void)vm;
    (void)reserved;

    LOGI("JNI_OnLoad called - spawning init worker");
    // create detached thread
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int r = pthread_create(&t, &attr, init_worker, NULL);
    if (r != 0) {
        LOGE("pthread_create failed: %d", r);
        // fallback: do not block; we'll just log error
    } else {
        LOGI("init worker created");
    }
    pthread_attr_destroy(&attr);

    return JNI_VERSION_1_6;
}

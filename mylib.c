// mylib.c
#include <stdio.h>
#include <time.h>
#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_TAG "MYLIB"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, VA_ARGS)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, VA_ARGS)

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

    // try to ensure dir exists for Download case  
    if (i == 1) {  
        mkdir("/sdcard/Download", 0777);  
    }  

    FILE *f = fopen(p, "a");  
    if (!f) {  
        // couldn't open, try next  
        LOGI("cannot open %s for append (errno=%d), try next", p, errno);  
        continue;  
    }  

    // timestamp  
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

// will execute automatically when the library is loaded via dlopen/System.loadLibrary
attribute((constructor))
static void on_load(void) {
// simple message
const char *msg = "mylib loaded successfully";

// write to android log  
LOGI("%s", msg);  

// write to file (sdcard or fallback)  
write_log_file(msg);

}

// optional: also define JNI_OnLoad so library loaded by JNI also calls into it
#include <jni.h>
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
LOGI("JNI_OnLoad called");
write_log_file("JNI_OnLoad called");
return JNI_VERSION_1_6;

// overlaydemo_hwclayer.cpp
// Android 10: Dùng HWC để vẽ overlay đỏ 5s

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <ui/DisplayInfo.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>
#include <utils/StrongPointer.h>
#include <utils/RefBase.h>
#include <binder/IBinder.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

using namespace android;

static void segv_handler(int sig, siginfo_t *si, void *unused) {
    fprintf(stderr, "FATAL: signal %d at address %p\n", sig, si ? si->si_addr : NULL);
    _exit(128 + sig);
}

static void install_segv_handler() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sa.sa_sigaction = segv_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

static int get_surface_flinger_pid() {
    FILE *fp = popen("pidof surfaceflinger", "r");
    if (!fp) return -1;
    char buf[256];
    if (!fgets(buf, sizeof(buf), fp)) { pclose(fp); return -1; }
    pclose(fp);
    size_t len = strlen(buf);
    if (len && buf[len-1] == '\n') buf[len-1] = 0;
    char *tok = strtok(buf, " \t");
    while (tok) {
        char *endptr = NULL;
        long pid = strtol(tok, &endptr, 10);
        if (endptr != tok && pid > 0) return (int)pid;
        tok = strtok(NULL, " \t");
    }
    return -1;
}

static int join_namespace(int pid, const char* nsname) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/ns/%s", pid, nsname);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "WARN: open(%s): %s\n", path, strerror(errno));
        return -1;
    }
    if (setns(fd, 0) < 0) {
        fprintf(stderr, "WARN: setns(%s): %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    fprintf(stdout, "INFO: joined %s ns\n", nsname);
    return 0;
}

int main(int argc, char** argv) {
    install_segv_handler();

    fprintf(stdout, "INFO: Starting overlaydemo_hwclayer (uid=%d,gid=%d)\n", getuid(), getgid());

    int pid = get_surface_flinger_pid();
    if (pid <= 0) { fprintf(stderr, "ERROR: cannot find surfaceflinger pid\n"); return -1; }
    fprintf(stdout, "INFO: surfaceflinger pid=%d\n", pid);

    join_namespace(pid, "mnt");
    join_namespace(pid, "ipc");
    join_namespace(pid, "net");

    if (setgid(1000) != 0) {
        fprintf(stderr, "WARN: setgid(1000) failed: %s\n", strerror(errno));
    }
    if (setuid(1000) != 0) {
        fprintf(stderr, "WARN: setuid(1000) failed: %s\n", strerror(errno));
    }
    fprintf(stdout, "INFO: after setuid -> uid=%d,gid=%d\n", getuid(), getgid());

    sp<SurfaceComposerClient> client = new SurfaceComposerClient();
    status_t st = client->initCheck();
    if (st != NO_ERROR) {
        fprintf(stderr, "ERROR: SurfaceComposerClient initCheck failed: %d\n", st);
        return -1;
    }
    fprintf(stdout, "INFO: connected to SurfaceFlinger\n");

    DisplayInfo info;
    sp<IBinder> display = SurfaceComposerClient::getInternalDisplayToken();
    if (display == nullptr) { fprintf(stderr, "ERROR: null display token\n"); return -1; }
    if (SurfaceComposerClient::getDisplayInfo(display, &info) != NO_ERROR) {
        fprintf(stderr, "ERROR: getDisplayInfo failed\n"); return -1;
    }
    int width = info.w, height = info.h;
    fprintf(stdout, "INFO: display %d x %d density=%f\n", width, height, info.density);

    // Tạo SurfaceControl với HWC layer
    sp<SurfaceControl> sc = client->createSurface(
        String8("RedOverlayHWC"),
        width, height,
        PIXEL_FORMAT_RGBA_8888,
        ISurfaceComposerClient::eFXSurfaceBufferQueue // Sử dụng BufferQueue cho HWC
    );
    if (sc == nullptr || !sc->isValid()) {
        fprintf(stderr, "ERROR: createSurface invalid\n");
        return -1;
    }

    SurfaceComposerClient::Transaction t;
    t.setLayer(sc, INT_MAX);
    t.show(sc);
    t.apply();

    // --- Khởi tạo HWC ---
    hw_module_t* module = nullptr;
    if (hw_get_module(HWC_HARDWARE_MODULE_ID, (const hw_module_t**)&module) != 0) {
        fprintf(stderr, "ERROR: cannot load HWC module\n");
        return -1;
    }

    hwc_composer_device_1_t* hwc_device = nullptr;
    if (module->methods->open(module, HWC_HARDWARE_COMPOSER, (hw_device_t**)&hwc_device) != 0) {
        fprintf(stderr, "ERROR: cannot open HWC device\n");
        return -1;
    }

    // Tạo buffer và vẽ màu đỏ
    hwc_display_contents_1_t* contents = nullptr;
    hwc_layer_1_t* layer = nullptr;
    // Lưu ý: Cấu hình HWC buffer và layer phụ thuộc vào thiết bị cụ thể
    // Code dưới đây là giả định, cần điều chỉnh theo HWC API thực tế của thiết bị
    buffer_handle_t buffer_handle = nullptr; // Cần cấp phát buffer từ HWC hoặc gralloc
    // Tạo buffer bằng gralloc hoặc cơ chế tương tự
    // Ví dụ: Dùng gralloc để cấp phát buffer
    hw_module_t* gralloc_module = nullptr;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t**)&gralloc_module) != 0) {
        fprintf(stderr, "ERROR: cannot load gralloc module\n");
        hwc_device->common.close((hw_device_t*)hwc_device);
        return -1;
    }

    alloc_device_t* alloc_device = nullptr;
    if (gralloc_module->methods->open(gralloc_module, GRALLOC_HARDWARE_GPU0, (hw_device_t**)&alloc_device) != 0) {
        fprintf(stderr, "ERROR: cannot open gralloc device\n");
        hwc_device->common.close((hw_device_t*)hwc_device);
        return -1;
    }

    buffer_handle_t buf = nullptr;
    int stride = 0;
    if (alloc_device->alloc(alloc_device, width, height, PIXEL_FORMAT_RGBA_8888, 
                            GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_COMPOSER, 
                            &buf, &stride) != 0) {
        fprintf(stderr, "ERROR: cannot allocate buffer\n");
        alloc_device->common.close((hw_device_t*)alloc_device);
        hwc_device->common.close((hw_device_t*)hwc_device);
        return -1;
    }

    // Lock và vẽ màu đỏ
    void* vaddr = nullptr;
    if (alloc_device->lock(alloc_device, buf, GRALLOC_USAGE_SW_WRITE_OFTEN, 0, 0, width, height, &vaddr) == 0 && vaddr) {
        uint32_t* pixels = reinterpret_cast<uint32_t*>(vaddr);
        for (int y = 0; y < height; y++) {
            uint32_t* row = pixels + (size_t)y * (size_t)stride;
            for (int x = 0; x < width; x++) {
                row[x] = 0xFFFF0000; // ARGB đỏ
            }
        }
        alloc_device->unlock(alloc_device, buf);
    } else {
        fprintf(stderr, "ERROR: cannot lock buffer\n");
    }

    // Gửi buffer tới HWC
    SurfaceComposerClient::Transaction bufT;
    bufT.setBuffer(sc, buf); // Gắn buffer vào SurfaceControl
    bufT.apply();
    fprintf(stdout, "INFO: buffer set and posted\n");

    fprintf(stdout, "INFO: sleeping 5s\n");
    sleep(5);

    // Cleanup
    SurfaceComposerClient::Transaction cleanup;
    cleanup.hide(sc);
    cleanup.reparent(sc, nullptr);
    cleanup.apply();

    if (buf) {
        alloc_device->free(alloc_device, buf);
    }
    if (alloc_device) {
        alloc_device->common.close((hw_device_t*)alloc_device);
    }
    if (hwc_device) {
        hwc_device->common.close((hw_device_t*)hwc_device);
    }

    fprintf(stdout, "INFO: exit.\n");
    return 0;
}

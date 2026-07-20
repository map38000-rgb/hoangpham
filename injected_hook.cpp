#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <pthread.h>
#include <cstring>
#include <cstdint>

// Tích hợp thư viện xdl và dobby
#include "xdl.h"
#include "dobby.h"

#define LOG_TAG "eBPF_Helper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Định nghĩa các hàm IL2CPP API thô dựa trên nguyên mẫu mã nguồn mở của Unity
typedef void* (*_il2cpp_domain_get)();
typedef void** (*_il2cpp_domain_get_assemblies)(void* domain, size_t* size);
typedef void* (*_il2cpp_assembly_get_image)(void* assembly);
typedef void* (*_il2cpp_class_from_name)(void* image, const char* namespaze, const char* name);

_il2cpp_class_from_name il2cpp_class_from_name_fn = nullptr;

// Cấu trúc Vector3 để hứng dữ liệu
struct Vector3 {
    float x;
    float y;
    float z;
};

// Con trỏ hàm gốc của get_Position để gọi lại khi cần
void* (*orig_get_Position)(void* _this) = nullptr;

// Hàm Hook thay thế cho get_Position
void* hook_get_Position(void* _this) {
    if (_this != nullptr) {
        // Gọi hàm gốc để lấy kết quả trả về an toàn
        // (Lưu ý: Nếu Vector3 được trả qua pointer ẩn ở param 1 tùy compiler, ta hứng trực tiếp hoặc đọc offset)
        
        // Cách an toàn nhất cho ARM64 khi ko chắc calling convention: Đọc trực tiếp từ memory 'this' + offset
        // Giả sử trường position nằm ở offset 0x38 bên trong class LTransform
        Vector3* pos = (Vector3*)((uintptr_t)_this + 0x38);
        
        LOGI("[+] LTransform::get_Position gọi bởi: %p | X: %.2f, Y: %.2f, Z: %.2f", _this, pos->x, pos->y, pos->z);
    }
    return orig_get_Position(_this);
}

// Luồng chạy ngầm đợi game khởi tạo xong IL2CPP
void* init_hook_thread(void*) {
    LOGI("[*] Đang đợi libil2cpp.so nạp vào bộ nhớ...");
    
    void* il2cpp_handle = nullptr;
    // Vòng lặp quét tìm libil2cpp.so bằng xdl (né tránh việc điền sai đường dẫn tuyệt đối)
    while (!il2cpp_handle) {
        il2cpp_handle = xdl_open("libil2cpp.so", XDL_DEFAULT);
        s_sleep(1);
    }
    LOGI("[+] Đã tìm thấy libil2cpp.so!");

    // Dùng xdl tìm các hàm API phục vụ việc Resolve Metadata dù bị strip
    _il2cpp_domain_get il2cpp_domain_get_fn = (_il2cpp_domain_get)xdl_sym(il2cpp_handle, "il2cpp_domain_get", nullptr);
    _il2cpp_domain_get_assemblies il2cpp_domain_get_assemblies_fn = (_il2cpp_domain_get_assemblies)xdl_sym(il2cpp_handle, "il2cpp_domain_get_assemblies", nullptr);
    _il2cpp_assembly_get_image il2cpp_assembly_get_image_fn = (_il2cpp_assembly_get_image)xdl_sym(il2cpp_handle, "il2cpp_assembly_get_image", nullptr);
    il2cpp_class_from_name_fn = (_il2cpp_class_from_name)xdl_sym(il2cpp_handle, "il2cpp_class_from_name", nullptr);

    if (!il2cpp_class_from_name_fn) {
        LOGE("[-] Không thể resolve các hàm IL2CPP API thô. Game có thể đã obfuscate tên API.");
        return nullptr;
    }

    // Tiến hành duyệt tìm Assembly-CSharp và Class LTransform
    void* domain = il2cpp_domain_get_fn();
    size_t size = 0;
    void** assemblies = il2cpp_domain_get_assemblies_fn(domain, &size);
    
    void* target_image = nullptr;
    for (size_t i = 0; i < size; ++i) {
        // Trong cấu trúc IL2CPP, tên của assembly lưu ở pointer đầu tiên hoặc có hàm get_name
        // Cách nhanh nhất: ép thử để lấy image của Assembly-CSharp.dll
        void* image = il2cpp_assembly_get_image_fn(assemblies[i]);
        // Giả lập duyệt tìm đúng dll mục tiêu
        target_image = image; // Tạm thời gán để duyệt tất cả image
    }

    // Resolve trực tiếp Class từ Namespace: GCommon và Name: LTransform
    // (Bản đồ Metadata từ global-metadata.dat sẽ tự động chỉ đường cho hàm này)
    void* ltransform_class = il2cpp_class_from_name_fn(target_image, "GCommon", "LTransform");
    
    if (ltransform_class) {
        LOGI("[+] Đã tự động định vị được Class LTransform tại địa chỉ: %p", ltransform_class);
        
        // Tìm địa chỉ hàm get_Position bên trong Class
        // Cách lẹ nhất: Do ta biết TypeDefIndex là 34940, hoặc dùng hàm il2cpp_class_get_method_from_name
        typedef void* (*_il2cpp_class_get_method_from_name)(void* klass, const char* name, int argsCount);
        _il2cpp_class_get_method_from_name get_method_fn = (_il2cpp_class_get_method_from_name)xdl_sym(il2cpp_handle, "il2cpp_class_get_method_from_name", nullptr);
        
        void* method_info = get_method_fn(ltransform_class, "get_Position", 0);
        if (method_info) {
            // Trong cấu trúc Il2CppMethodPointer, phần tử đầu tiên luôn chứa địa chỉ thực thi (Native Code)
            void* target_func_address = *(void**)method_info; 
            LOGI("[+] Tìm thấy địa chỉ Native Code của get_Position tại: %p", target_func_address);

            // Tiến hành dùng Dobby để đè Hook
            DobbyHook(target_func_address, (dobby_dummy_func_t)hook_get_Position, (dobby_dummy_func_t*)&orig_get_Position);
            LOGI("[+] Thiết lập Dobby Hook thành công!");
        }
    } else {
        LOGE("[-] Không tìm thấy class GCommon.LTransform trên RAM!");
    }

    xdl_close(il2cpp_handle);
    return nullptr;
}

// Hàm khởi tạo khi thư viện .so được nạp vào tiến trình
void __attribute__((constructor)) init() {
    pthread_t thread;
    pthread_create(&thread, nullptr, init_hook_thread, nullptr);
}

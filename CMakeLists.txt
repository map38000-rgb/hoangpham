cmake_minimum_required(VERSION 3.10)
project(camera_dobby_hook C)

# Bật compile chuẩn GNU cho Android
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# In ra thông tin build
message(STATUS "ABI: ${ANDROID_ABI}")
message(STATUS "Platform: ${ANDROID_PLATFORM}")
message(STATUS "NDK: ${ANDROID_NDK}")

# ==== Đường dẫn tới Dobby ====
# Giả sử bạn có thư mục: dobby/include/ và dobby/libs/<ABI>/libdobby.a
# Nếu dùng Dobby submodule đã build bằng workflow thì sẽ có lib ở build/dobby/<ABI>/
set(DOBBY_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/dobby/include)
set(DOBBY_LIB_DIR ${CMAKE_SOURCE_DIR}/dobby/libs/${ANDROID_ABI})

# Nếu không tìm thấy, thử fallback sang build/dobby
if(NOT EXISTS "${DOBBY_LIB_DIR}/libdobby.a")
    set(DOBBY_LIB_DIR ${CMAKE_SOURCE_DIR}/build/dobby/${ANDROID_ABI})
endif()

# In ra đường dẫn thực tế
message(STATUS "Dobby include: ${DOBBY_INCLUDE_DIR}")
message(STATUS "Dobby lib: ${DOBBY_LIB_DIR}")

# ==== Thêm include path ====
include_directories(${DOBBY_INCLUDE_DIR})

# ==== File nguồn chính ====
add_library(camera_dobby_hook SHARED
    camera_dobby_hook.c
)

# ==== Liên kết với Dobby ====
# Nếu có static lib
if(EXISTS "${DOBBY_LIB_DIR}/libdobby.a")
    target_link_libraries(camera_dobby_hook
        ${DOBBY_LIB_DIR}/libdobby.a
        dl
        log
    )
    message(STATUS "Linked static Dobby: ${DOBBY_LIB_DIR}/libdobby.a")
else()
    # Nếu không có static lib, link động (phải có libdobby.so ở runtime)
    target_link_libraries(camera_dobby_hook
        dobby
        dl
        log
    )
    message(WARNING "⚠️  Không tìm thấy libdobby.a — sẽ link động (-ldobby). 
    Bạn cần đảm bảo libdobby.so có sẵn khi chạy.")
endif()

# ==== Cờ biên dịch tối ưu ====
target_compile_options(camera_dobby_hook PRIVATE
    -fPIC
    -O2
    -D_GNU_SOURCE
    -Wall
)

# ==== Đường dẫn xuất kết quả ====
set_target_properties(camera_dobby_hook PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/output/${ANDROID_ABI}
)

# ==== Log ====
message(STATUS "Output: ${CMAKE_SOURCE_DIR}/build/output/${ANDROID_ABI}/camera_dobby_hook.so")

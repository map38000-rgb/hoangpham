cmake_minimum_required(VERSION 3.10)

# Kích hoạt hỗ trợ cả C, C++ và Assembly (ASM) trong dự án
project(ebpf_helper C CXX ASM)

set(CMAKE_CXX_STANDARD 11)

# Ép cờ cấu hình Assembly cho Android NDK trước khi load Dobby
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} --target=aarch64-linux-android")

# Cấu hình các cờ ép Dobby nhận diện đúng Android ARM64 trước khi import
set(DOBBY_GENERATE_SHARED OFF CACHE INTERNAL "")
set(DOBBY_BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

# Nhúng trực tiếp mã nguồn Dobby vào cây build của dự án chính
add_subdirectory(dobby EXCLUDE_FROM_ALL)

# Đón nhận đường dẫn động được truyền từ lệnh build của GitHub Actions
if(DEFINED DT_XDL_SRC AND DEFINED DT_XDL_INC)
    set(XDL_SRC ${DT_XDL_SRC})
    set(XDL_INC ${DT_XDL_INC})
else()
    set(XDL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src/xdl.c")
    set(XDL_INC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src")
endif()

# Include headers của Dobby và XDL
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby/include)
include_directories(${XDL_INC})

# Biên dịch tệp nguồn của bạn kèm theo xdl
add_library(ebpf_helper SHARED 
    injected_hook.cpp
    ${XDL_SRC}
)

# Định nghĩa các macro cần thiết để xdl hoạt động mượt mà
target_compile_definitions(ebpf_helper PRIVATE XDL_PREFIX=xdl)

# Liên kết trực tiếp target 'dobby' tĩnh vừa được build đồng bộ và thư viện log Android
target_link_libraries(ebpf_helper log dobby)

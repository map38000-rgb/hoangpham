cmake_minimum_required(VERSION 3.10)
project(ebpf_helper)

set(CMAKE_CXX_STANDARD 11)

# Cấu hình các cờ ép Dobby nhận diện đúng Android ARM64 trước khi import
set(DOBBY_GENERATE_SHARED OFF CACHE INTERNAL "")
set(DOBBY_BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

# Nhúng trực tiếp mã nguồn Dobby vào cây build của dự án chính
add_subdirectory(dobby EXCLUDE_FROM_ALL)

# Include headers của Dobby và XDL
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/xdl/src)

# Biên dịch tệp nguồn của bạn kèm theo xdl
add_library(ebpf_helper SHARED 
    injected_hook.cpp
    xdl/src/xdl.c
)

# Định nghĩa các macro cần thiết để xdl hoạt động mượt mà
target_compile_definitions(ebpf_helper PRIVATE XDL_PREFIX=xdl)

# Liên kết trực tiếp target 'dobby' tĩnh vừa được build đồng bộ và thư viện log Android
target_link_libraries(ebpf_helper log dobby)

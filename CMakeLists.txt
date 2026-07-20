cmake_minimum_required(VERSION 3.10)
project(ebpf_helper)

set(CMAKE_CXX_STANDARD 11)

# Chỉ đường dẫn chính xác tới các file header vừa được Git Action tải về
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/xdl/src) # xdl dùng src làm nơi chứa header chính

# Thêm source code của bạn và cả file nguồn của xdl để build chung (né lỗi thiếu thư viện liên kết)
add_library(ebpf_helper SHARED 
    injected_hook.cpp
    xdl/src/xdl.c # Biên dịch kèm file nguồn của xdl
)

# Liên kết với file thư viện tĩnh (.a) của Dobby
# Thường CMake của Dobby sẽ tự tạo mục tiêu phụ, hoặc bạn add thẳng sub_directory:
add_subdirectory(dobby build_dobby)

target_link_libraries(ebpf_helper log dobby)

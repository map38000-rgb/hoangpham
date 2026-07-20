cmake_minimum_required(VERSION 3.10)
project(ebpf_helper)

set(CMAKE_CXX_STANDARD 11)

# Đường dẫn tới thư mục chứa Dobby và XDL tương ứng trong dự án của bạn
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/xdl/include)

add_library(ebpf_helper SHARED injected_hook.cpp)

# Liên kết với các thư viện log của Android hệ thống
target_link_libraries(ebpf_helper log)
# Thêm file prebuilt hoặc source code của dobby/xdl vào đây tùy cách bạn tổ chức thư mục

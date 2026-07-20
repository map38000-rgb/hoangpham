cmake_minimum_required(VERSION 3.10)
project(ebpf_helper)

set(CMAKE_CXX_STANDARD 11)

# Cấu hình các cờ ép Dobby nhận diện đúng Android ARM64 trước khi import
set(DOBBY_GENERATE_SHARED OFF CACHE INTERNAL "")
set(DOBBY_BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

# Nhúng trực tiếp mã nguồn Dobby vào cây build của dự án chính
add_subdirectory(dobby EXCLUDE_FROM_ALL)

# Tự động kiểm tra vị trí chính xác của file xdl.c (ở gốc hoặc trong src)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/xdl/xdl.c")
    set(XDL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/xdl.c")
    set(XDL_INC "${CMAKE_CURRENT_SOURCE_DIR}/xdl")
else()
    set(XDL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src/xdl.c")
    set(XDL_INC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src")
endif()

# Include headers của Dobby và XDL
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby/include)
include_directories(${XDL_INC})

# Biên dịch tệp nguồn của bạn kèm theo xdl với đường dẫn động vừa tìm được
add_library(ebpf_helper SHARED 
    injected_hook.cpp
    ${XDL_SRC}
)

# Định nghĩa các macro cần thiết để xdl hoạt động mượt mà
target_compile_definitions(ebpf_helper PRIVATE XDL_PREFIX=xdl)

# Liên kết trực tiếp target 'dobby' tĩnh vừa được build đồng bộ và thư viện log Android
target_link_libraries(ebpf_helper log dobby)

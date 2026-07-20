cmake_minimum_required(VERSION 3.10)

# Kích hoạt đầy đủ các ngôn ngữ
project(ebpf_helper C CXX ASM)

set(CMAKE_CXX_STANDARD 11)

# Khóa cứng cấu hình Android để Dobby không bị nhận diện nhầm sang Linux host
set(ANDROID TRUE CACHE INTERNAL "")
set(OS_ANDROID CONFIG_TRUE CACHE INTERNAL "")
set(OS_LINUX CONFIG_FALSE CACHE INTERNAL "")

set(DOBBY_GENERATE_SHARED OFF CACHE INTERNAL "")
set(DOBBY_BUILD_TESTING OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

# Nhúng trực tiếp mã nguồn Dobby
add_subdirectory(dobby EXCLUDE_FROM_ALL)

# Cấu hình đường dẫn XDL
if(DEFINED DT_XDL_SRC AND DEFINED DT_XDL_INC)
    set(XDL_SRC ${DT_XDL_SRC})
    set(XDL_INC ${DT_XDL_INC})
else()
    set(XDL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src/xdl.c")
    set(XDL_INC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby/include)
include_directories(${XDL_INC})

# Tạo file thư viện .so chính
add_library(ebpf_helper SHARED 
    injected_hook.cpp
    ${XDL_SRC}
)

target_compile_definitions(ebpf_helper PRIVATE XDL_PREFIX=xdl)
target_link_libraries(ebpf_helper log dobby)

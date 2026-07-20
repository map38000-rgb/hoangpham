cmake_minimum_required(VERSION 3.10)
project(ebpf_helper C CXX)

set(CMAKE_CXX_STANDARD 11)

# Nhận đường dẫn động từ workflow
if(DEFINED DT_XDL_SRC AND DEFINED DT_XDL_INC)
    set(XDL_SRC ${DT_XDL_SRC})
    set(XDL_INC ${DT_XDL_INC})
else()
    set(XDL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src/xdl.c")
    set(XDL_INC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/include")
endif()

# Chỉ định đường dẫn chứa file libdobby.a tĩnh
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby_prebuilt)

# Tạo target thư viện .so chính
add_library(ebpf_helper SHARED 
    injected_hook.cpp
    ${XDL_SRC}
)

# Áp include đường dẫn chứa xdl.h cho toàn bộ các file nguồn biên dịch
target_include_directories(ebpf_helper PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/dobby_prebuilt
    ${XDL_INC}
)

# Định nghĩa macro cần thiết của xdl
target_compile_definitions(ebpf_helper PRIVATE XDL_PREFIX=xdl)

# Link thư viện tĩnh dobby và log hệ thống
target_link_libraries(ebpf_helper log dobby)

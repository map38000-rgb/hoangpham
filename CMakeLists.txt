cmake_minimum_required(VERSION 3.10)
project(ebpf_helper C CXX)

set(CMAKE_CXX_STANDARD 11)

# Đón nhận đường dẫn động của XDL được truyền từ GitHub Actions
if(DEFINED DT_XDL_SRC AND DEFINED DT_XDL_INC)
    set(XDL_SRC ${DT_XDL_SRC})
    set(XDL_INC ${DT_XDL_INC})
else()
    set(XDL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src/xdl.c")
    set(XDL_INC "${CMAKE_CURRENT_SOURCE_DIR}/xdl/src")
endif()

# Chỉ định đường dẫn chứa file libdobby.a tĩnh
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/dobby_prebuilt)

# Tạo target thư viện .so chính
add_library(ebpf_helper SHARED 
    injected_hook.cpp
    ${XDL_SRC}
)

# Ép hệ thống include header cho TẤT CẢ các file thuộc target (Fix lỗi fatal error: 'xdl.h' file not found)
target_include_directories(ebpf_helper PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/dobby_prebuilt
    ${XDL_INC}
)

# Định nghĩa các macro cần thiết để xdl hoạt động mượt mà
target_compile_definitions(ebpf_helper PRIVATE XDL_PREFIX=xdl)

# Liên kết trực tiếp thư viện log Android và file static dobby đã tải về
target_link_libraries(ebpf_helper log dobby)

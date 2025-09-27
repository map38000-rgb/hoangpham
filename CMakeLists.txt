cmake_minimum_required(VERSION 3.6)

project(mylib C)

set(CMAKE_C_STANDARD 99)

add_library(mylib SHARED mylib.c)

# Link với log (bắt buộc để dùng __android_log_print)
find_library(log-lib log)
target_link_libraries(mylib ${log-lib})

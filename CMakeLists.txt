cmake_minimum_required(VERSION 3.10)

project(overlaydemo_hwclayer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(overlaydemo_hwclayer overlaydemo_hwclayer.cpp)

# Link Android system libraries
target_link_libraries(overlaydemo_hwclayer
    log
    android
)

cmake_minimum_required(VERSION 3.10.2)
project(UltraClock)

add_library(main SHARED
    ../../../../src/main.cpp
    ../../../../src/ntp_client.cpp
    ../../../../src/precise_clock.cpp
    ../../../../src/renderer.cpp
)

find_library(log-lib log)
find_package(SDL2 REQUIRED)

target_include_directories(main PRIVATE ../../../../src ../../../../src/stb)
target_link_libraries(main ${log-lib} SDL2::SDL2)

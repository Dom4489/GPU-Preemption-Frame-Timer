cmake_minimum_required(VERSION 3.16)
project(FrameTimer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(frametimer
    src/main.cpp
    include/Clock.cpp
    include/FrameTimer.cpp
    include/Kernelbridge.cpp
)

target_include_directories(frametimer PRIVATE include)

if(WIN32)
    target_link_libraries(frametimer PRIVATE kernel32)
    # Static linking: bundles MinGW runtime libraries into exe
    target_link_options(frametimer PRIVATE -static)
endif()
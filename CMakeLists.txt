cmake_minimum_required(VERSION 3.16)
project(FrameTimer CXX)
 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
# Strict warnings — good habit for driver-adjacent C++ work
if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
 
add_executable(frametimer
    src/main.cpp
    include/Clock.cpp
)
 
target_include_directories(frametimer PRIVATE include)
 
# On Windows, link against the kernel timing libs.
# No extra libs needed on Mac (std::chrono is in libc++).
if(WIN32)
    target_link_libraries(frametimer PRIVATE kernel32)
endif()
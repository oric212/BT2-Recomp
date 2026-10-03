cmake_minimum_required(VERSION 3.20)
project(BT2Recomp VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "BT2 Recomp requires Windows and MSVC (Visual Studio 2022).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "BT2 Recomp requires x64. Configure with -A x64 in a fresh build directory.")
endif()

add_executable(bt2-port runtime/main.cpp)
set_target_properties(bt2-port PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_compile_options(bt2-port PRIVATE /W4 /permissive- /utf-8)

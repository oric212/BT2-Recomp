cmake_minimum_required(VERSION 3.20)
project(BT2Recomp VERSION 0.1.0 LANGUAGES CXX)

add_executable(bt2-recomp src/main.cpp)
target_compile_features(bt2-recomp PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(bt2-recomp PRIVATE /W4 /permissive-)
else()
    target_compile_options(bt2-recomp PRIVATE -Wall -Wextra -Wpedantic)
endif()

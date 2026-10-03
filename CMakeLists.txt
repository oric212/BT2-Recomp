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

option(BT2_BUILD_GENERATED_PROBE "Build the isolated local generated-code probe" OFF)
if(BT2_BUILD_GENERATED_PROBE)
    set(BT2_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2" CACHE PATH "Ignored generated BT2 sources")
    set(BT2_PROBE_SOURCE "${BT2_GENERATED_DIR}/FUN_0032EEE0_0x32eee0.cpp")
    if(NOT EXISTS "${BT2_PROBE_SOURCE}")
        message(FATAL_ERROR "Generate local BT2 code with tools/ps2recomp/run_pipeline.py first.")
    endif()
    add_executable(bt2-generated-probe runtime/generated_probe.cpp "${BT2_PROBE_SOURCE}")
    target_include_directories(bt2-generated-probe PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-generated-probe PRIVATE cxx_std_20)
    target_compile_options(bt2-generated-probe PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-generated-probe PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    enable_testing()
    add_test(NAME generated-bt2-probe COMMAND bt2-generated-probe)
    # Diagnostic target: expose the first generated-code ABI blocker without breaking the host/probe.
    # Start at entry: MSVC otherwise continues compiling thousands of files after the first ABI error.
    add_library(bt2-generated-compile-check OBJECT EXCLUDE_FROM_ALL "${BT2_GENERATED_DIR}/FUN_00100008_0x100008.cpp")
    target_include_directories(bt2-generated-compile-check PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-generated-compile-check PRIVATE cxx_std_20)
    target_compile_options(bt2-generated-compile-check PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-generated-compile-check PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
endif()

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
    add_executable(bt2-bootstrap-tests runtime/bootstrap_tests.cpp)
    target_include_directories(bt2-bootstrap-tests PRIVATE runtime/probe_abi)
    target_compile_features(bt2-bootstrap-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-bootstrap-tests PRIVATE /W4 /permissive- /utf-8)
    add_test(NAME bootstrap-runtime COMMAND bt2-bootstrap-tests)
    add_executable(bt2-kernel-handler-tests runtime/kernel_handler_tests.cpp
        "${BT2_GENERATED_DIR}/FUN_00338388_0x338388.cpp")
    target_include_directories(bt2-kernel-handler-tests PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-kernel-handler-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-kernel-handler-tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-kernel-handler-tests PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    add_test(NAME generated-kernel-handler COMMAND bt2-kernel-handler-tests)
    add_executable(bt2-buffer-fill-tests runtime/buffer_fill_tests.cpp "${BT2_GENERATED_DIR}/FUN_003251DC_0x3251dc.cpp")
    target_include_directories(bt2-buffer-fill-tests PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-buffer-fill-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-buffer-fill-tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-buffer-fill-tests PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    add_test(NAME generated-buffer-fill COMMAND bt2-buffer-fill-tests)
    add_executable(bt2-timer-write-tests runtime/timer_write_tests.cpp "${BT2_GENERATED_DIR}/FUN_00338AD8_0x338ad8.cpp")
    target_include_directories(bt2-timer-write-tests PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-timer-write-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-timer-write-tests PRIVATE /W4 /permissive- /utf-8)
    add_test(NAME generated-timer-write COMMAND bt2-timer-write-tests)
    # Compile/link entry separately so future blockers do not break the independent host/probe.
    add_executable(bt2-entry-probe EXCLUDE_FROM_ALL runtime/entry_probe.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00100008_0x100008.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00338520_0x338520.cpp"
        "${BT2_GENERATED_DIR}/FUN_003382D0_0x3382d0.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EC60_0x32ec60.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338410_0x338410.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338510_0x338510.cpp"
        "${BT2_GENERATED_DIR}/FUN_003383C8_0x3383c8.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338388_0x338388.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338A00_0x338a00.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338998_0x338998.cpp"
        "${BT2_GENERATED_DIR}/FUN_003389A8_0x3389a8.cpp"
        "${BT2_GENERATED_DIR}/FUN_003389B8_0x3389b8.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EEE0_0x32eee0.cpp"
        "${BT2_GENERATED_DIR}/FUN_003389F0_0x3389f0.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338B78_0x338b78.cpp"
        "${BT2_GENERATED_DIR}/FUN_003251DC_0x3251dc.cpp"
        "${BT2_GENERATED_DIR}/FUN_00339E48_0x339e48.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032E950_0x32e950.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338268_0x338268.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338B58_0x338b58.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338AD8_0x338ad8.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032F7A8_0x32f7a8.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032E9A0_0x32e9a0.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338DB8_0x338db8.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032FA38_0x32fa38.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EA60_0x32ea60.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032FDA0_0x32fda0.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032F730_0x32f730.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EB50_0x32eb50.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EAF0_0x32eaf0.cpp")
    option(BT2_BUILD_KERNEL_PAYLOAD "Link the scoped ignored copied-kernel entry" OFF)
    if(BT2_BUILD_KERNEL_PAYLOAD)
        set(BT2_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-kernel-runtime")
        target_sources(bt2-entry-probe PRIVATE "${BT2_KERNEL_DIR}/FUN_80076000_0x80076000.cpp")
        target_include_directories(bt2-entry-probe PRIVATE "${BT2_KERNEL_DIR}")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_KERNEL_PAYLOAD=1)
    endif()
    target_include_directories(bt2-entry-probe PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-entry-probe PRIVATE cxx_std_20)
    target_compile_options(bt2-entry-probe PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-entry-probe PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    add_test(NAME generated-entry-probe COMMAND bt2-entry-probe
        "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/guest-memory.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/entry-stop.json")
endif()

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
    add_executable(bt2-gs-command-tests runtime/gs_command_tests.cpp)
    target_include_directories(bt2-gs-command-tests PRIVATE runtime/probe_abi)
    target_compile_features(bt2-gs-command-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-gs-command-tests PRIVATE /W4 /permissive- /utf-8)
    add_test(NAME gs-command-consumption COMMAND bt2-gs-command-tests)
    add_executable(bt2-post-sif-tests runtime/post_reset_sif_tests.cpp)
    target_include_directories(bt2-post-sif-tests PRIVATE runtime/probe_abi)
    target_compile_features(bt2-post-sif-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-post-sif-tests PRIVATE /W4 /permissive- /utf-8)
    add_test(NAME post-reset-sif COMMAND bt2-post-sif-tests)
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
    add_executable(bt2-copy-handler-tests runtime/copy_handler_tests.cpp "${BT2_GENERATED_DIR}/FUN_0033A7C8_0x33a7c8.cpp")
    target_include_directories(bt2-copy-handler-tests PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-copy-handler-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-copy-handler-tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-copy-handler-tests PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    add_test(NAME generated-copy-handler COMMAND bt2-copy-handler-tests)
    add_executable(bt2-compare-bytes-tests runtime/compare_bytes_tests.cpp "${BT2_GENERATED_DIR}/FUN_00325098_0x325098.cpp" "${BT2_GENERATED_DIR}/FUN_00326270_0x326270.cpp")
    target_include_directories(bt2-compare-bytes-tests PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-compare-bytes-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-compare-bytes-tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-compare-bytes-tests PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    add_test(NAME generated-byte-comparison COMMAND bt2-compare-bytes-tests)
    # Independent response validation; no entry-path interrupt delivery.
    add_executable(bt2-rpc-bind-probe runtime/rpc_bind_probe.cpp
        "${BT2_GENERATED_DIR}/FUN_003322E8_0x3322e8.cpp"
        "${BT2_GENERATED_DIR}/FUN_00332258_0x332258.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EC90_0x32ec90.cpp")
    target_include_directories(bt2-rpc-bind-probe PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-rpc-bind-probe PRIVATE cxx_std_20)
    target_compile_options(bt2-rpc-bind-probe PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-rpc-bind-probe PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    # Compile/link entry separately so future blockers do not break the independent host/probe.
    add_executable(bt2-entry-probe EXCLUDE_FROM_ALL runtime/entry_probe.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00100008_0x100008.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00338520_0x338520.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_0033A238_0x33a238.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00321E50_0x321e50.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00321D90_0x321d90.cpp"
        "${BT2_GENERATED_DIR}/FUN_00322F90_0x322f90.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EC80_0x32ec80.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032ECA0_0x32eca0.cpp"
        "${BT2_GENERATED_DIR}/FUN_003242D0_0x3242d0.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00331FE8_0x331fe8.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00331888_0x331888.cpp"
        "${BT2_GENERATED_DIR}/FUN_003382B8_0x3382b8.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032E970_0x32e970.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032F878_0x32f878.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032E9C0_0x32e9c0.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032F070_0x32f070.cpp"
        "${BT2_GENERATED_DIR}/FUN_00331D70_0x331d70.cpp"
        "${BT2_GENERATED_DIR}/FUN_00331C38_0x331c38.cpp"
        "${BT2_GENERATED_DIR}/FUN_00331B70_0x331b70.cpp"
        "${BT2_GENERATED_DIR}/FUN_0031F8B0_0x31f8b0.cpp"
        "${BT2_GENERATED_DIR}/FUN_0031F638_0x31f638.cpp"
        "${BT2_GENERATED_DIR}/FUN_00332B70_0x332b70.cpp"
        "${BT2_GENERATED_DIR}/FUN_00332790_0x332790.cpp"
        "${BT2_GENERATED_DIR}/FUN_003321B0_0x3321b0.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032F020_0x32f020.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-runtime/FUN_00331F38_0x331f38.cpp"
        "${BT2_GENERATED_DIR}/FUN_00100388_0x100388.cpp"
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
        "${BT2_GENERATED_DIR}/FUN_0032EAF0_0x32eaf0.cpp"
        "${BT2_GENERATED_DIR}/FUN_00338648_0x338648.cpp"
        "${BT2_GENERATED_DIR}/FUN_003385E0_0x3385e0.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032ED10_0x32ed10.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032ED00_0x32ed00.cpp"
        "${BT2_GENERATED_DIR}/FUN_0033A820_0x33a820.cpp"
        "${BT2_GENERATED_DIR}/FUN_0033A810_0x33a810.cpp"
        "${BT2_GENERATED_DIR}/FUN_0033A7B8_0x33a7b8.cpp"
        "${BT2_GENERATED_DIR}/FUN_0033A7C8_0x33a7c8.cpp"
        "${BT2_GENERATED_DIR}/FUN_0033A800_0x33a800.cpp")
    target_sources(bt2-entry-probe PRIVATE
        "${BT2_GENERATED_DIR}/FUN_0031EDB0_0x31edb0.cpp"
        "${BT2_GENERATED_DIR}/FUN_0031F010_0x31f010.cpp"
        "${BT2_GENERATED_DIR}/FUN_00332970_0x332970.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EC70_0x32ec70.cpp"
        "${BT2_GENERATED_DIR}/FUN_003322E8_0x3322e8.cpp"
        "${BT2_GENERATED_DIR}/FUN_00332258_0x332258.cpp"
        "${BT2_GENERATED_DIR}/FUN_0032EC90_0x32ec90.cpp")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/extra-startup.cmake")
        include("${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/extra-startup.cmake")
        target_include_directories(bt2-entry-probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_EXTRA_STARTUP_BINDINGS=1)
    endif()
    option(BT2_BUILD_KERNEL_PAYLOAD "Link the scoped ignored copied-kernel entry" OFF)
    if(BT2_BUILD_KERNEL_PAYLOAD)
        set(BT2_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-kernel-runtime")
        target_sources(bt2-entry-probe PRIVATE "${BT2_KERNEL_DIR}/FUN_80076000_0x80076000.cpp")
        target_include_directories(bt2-entry-probe PRIVATE "${BT2_KERNEL_DIR}")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_KERNEL_PAYLOAD=1)
    endif()
    option(BT2_BUILD_STARTUP_KERNEL_PAYLOAD "Link the scoped ignored startup kernel lookup" OFF)
    if(BT2_BUILD_STARTUP_KERNEL_PAYLOAD)
        if(NOT BT2_BUILD_KERNEL_PAYLOAD)
            message(FATAL_ERROR "Startup kernel payload requires primary kernel payload")
        endif()
        set(BT2_STARTUP_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/bt2-kernel-runtime-startup")
        target_sources(bt2-entry-probe PRIVATE "${BT2_STARTUP_KERNEL_DIR}/FUN_80075000_0x80075000.cpp")
        target_include_directories(bt2-entry-probe PRIVATE "${BT2_STARTUP_KERNEL_DIR}")
        target_compile_definitions(bt2-entry-probe PRIVATE BT2_STARTUP_KERNEL_PAYLOAD=1)
    endif()
    target_include_directories(bt2-entry-probe PRIVATE runtime/probe_abi "${BT2_GENERATED_DIR}")
    target_compile_features(bt2-entry-probe PRIVATE cxx_std_20)
    target_compile_options(bt2-entry-probe PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-entry-probe PRIVATE PS2X_STRICT_RETURN_DIAGNOSTICS=1)
    add_test(NAME generated-entry-probe COMMAND bt2-entry-probe
        "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/guest-memory.bin"
        "${CMAKE_CURRENT_SOURCE_DIR}/work/bt2-generation/entry-stop.json")
endif()

option(BT2_BUILD_IOP_PROBE "Build isolated locally generated MIPS-I module probe" OFF)
if(BT2_BUILD_IOP_PROBE)
    enable_testing()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2man_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcman_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcserv_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/dbcman_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2d_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/ds2ud_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/libsd_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sdrdrv_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modhsyn_native.cpp" OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modsesq2_native.cpp")
        message(FATAL_ERROR "Generate the local module with tools/iop/generate_native.py first.")
    endif()
    add_custom_command(OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/synthetic_native.cpp"
        COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/iop/make_synthetic.py"
        DEPENDS tools/iop/make_synthetic.py tools/iop/generate_native.py)
    add_executable(bt2-iop-tests runtime/iop/native_tests.cpp "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/synthetic_native.cpp")
    add_executable(bt2-iop-rpc-tests runtime/iop/rpc_tests.cpp)
    add_executable(bt2-iop-probe runtime/iop/module_probe.cpp runtime/iop/compiled_modules.cpp "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2man_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcserv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/dbcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2d_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/ds2ud_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/libsd_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sdrdrv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modhsyn_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modsesq2_native.cpp")
    add_executable(bt2-iop-service-tests runtime/iop/service_tests.cpp runtime/iop/compiled_modules.cpp "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2man_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcserv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/dbcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2d_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/ds2ud_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/libsd_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sdrdrv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modhsyn_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modsesq2_native.cpp")
    foreach(bt2_iop_target bt2-iop-tests bt2-iop-probe bt2-iop-service-tests bt2-iop-rpc-tests)
        target_include_directories(${bt2_iop_target} PRIVATE runtime/iop "${CMAKE_CURRENT_SOURCE_DIR}/work/iop")
        target_compile_features(${bt2_iop_target} PRIVATE cxx_std_20)
        target_compile_options(${bt2_iop_target} PRIVATE /W4 /permissive- /utf-8)
    endforeach()
    add_test(NAME native-iop-synthetic COMMAND bt2-iop-tests)
    add_test(NAME native-iop-rpc COMMAND bt2-iop-rpc-tests)
    add_test(NAME native-iop-service COMMAND bt2-iop-service-tests)
endif()

option(BT2_LINK_NATIVE_IOP "Connect compiled owned IOP modules to the EE LOADFILE probe" OFF)
if(BT2_LINK_NATIVE_IOP)
    if(NOT TARGET bt2-entry-probe)
        message(FATAL_ERROR "Native IOP bridge requires the generated entry probe")
    endif()
    target_sources(bt2-entry-probe PRIVATE runtime/iop/ee_loadfile_bridge.cpp runtime/iop/compiled_modules.cpp "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2man_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcserv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/dbcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2d_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/ds2ud_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/libsd_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sdrdrv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modhsyn_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modsesq2_native.cpp")
    target_include_directories(bt2-entry-probe PRIVATE runtime/iop "${CMAKE_CURRENT_SOURCE_DIR}/work/iop")
    target_compile_definitions(bt2-entry-probe PRIVATE BT2_NATIVE_IOP=1)
    add_executable(bt2-ee-iop-tests runtime/iop/ee_bridge_tests.cpp runtime/iop/ee_loadfile_bridge.cpp runtime/iop/compiled_modules.cpp "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2man_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/mcserv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/dbcman_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sio2d_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/ds2ud_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/libsd_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/sdrdrv_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modhsyn_native.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/work/iop/modsesq2_native.cpp")
    target_include_directories(bt2-ee-iop-tests PRIVATE runtime/probe_abi runtime/iop "${CMAKE_CURRENT_SOURCE_DIR}/work/iop")
    target_compile_features(bt2-ee-iop-tests PRIVATE cxx_std_20)
    target_compile_options(bt2-ee-iop-tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(bt2-ee-iop-tests PRIVATE BT2_NATIVE_IOP=1)
    add_test(NAME ee-native-iop-failure COMMAND bt2-ee-iop-tests)
endif()

# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 KL5NE, Alaska HamRadio, and AHR-Decoder contributors
cmake_minimum_required(VERSION 3.16)
project(ahrwwv_core CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Portable decoder engine: no Qt, no SDRangel. The SDRangel plugin links this.
add_library(ahrwwv_core STATIC core/src/timecode.cpp core/src/decoder.cpp)
target_include_directories(ahrwwv_core PUBLIC core/include)
set_target_properties(ahrwwv_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Plain C API for non-C++ hosts (Nexus via a Rust -sys crate).
add_library(ahrwwv_capi STATIC capi/src/ahr_wwv_capi.cpp)
target_include_directories(ahrwwv_capi PUBLIC capi/include)
target_link_libraries(ahrwwv_capi PUBLIC ahrwwv_core)
set_target_properties(ahrwwv_capi PROPERTIES POSITION_INDEPENDENT_CODE ON)

# TEST ONLY synthetic rig audio for host test suites (Nexus' Rust tests).
option(AHRWWV_CAPI_TEST_SYNTH "Build the test-only synthetic rig-audio library" OFF)
if(AHRWWV_CAPI_TEST_SYNTH)
    add_library(ahrwwv_testsynth STATIC capi/testsynth/ahr_wwv_test_synth.cpp sim/wwvsim.cpp)
    target_include_directories(ahrwwv_testsynth PUBLIC capi/testsynth sim)
    target_link_libraries(ahrwwv_testsynth PUBLIC ahrwwv_core)
    set_target_properties(ahrwwv_testsynth PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

option(AHRWWV_BUILD_TESTS "Build simulator, tests and tools" ON)
if(AHRWWV_BUILD_TESTS)
    add_library(ahrwwv_sim STATIC sim/wwvsim.cpp)
    target_include_directories(ahrwwv_sim PUBLIC sim)
    target_link_libraries(ahrwwv_sim PUBLIC ahrwwv_core)

    add_executable(ahrwwv_tests tests/test_main.cpp)
    target_link_libraries(ahrwwv_tests ahrwwv_sim)

    add_executable(ahrwwv_stress tests/stress.cpp)
    target_link_libraries(ahrwwv_stress ahrwwv_sim)

    add_executable(wwv_decode_wav tools/decode_wav.cpp)
    target_link_libraries(wwv_decode_wav ahrwwv_core)

    add_executable(wwv_make_sdriq tools/make_sdriq.cpp)
    target_link_libraries(wwv_make_sdriq ahrwwv_sim)

    add_executable(wwv_make_wav tools/make_wav.cpp)
    target_link_libraries(wwv_make_wav ahrwwv_sim)

    enable_language(C)
    add_executable(ahrwwv_capi_tests capi/tests/test_capi.cpp capi/tests/test_capi_c.c)
    set_source_files_properties(capi/tests/test_capi_c.c PROPERTIES LANGUAGE C)
    target_link_libraries(ahrwwv_capi_tests ahrwwv_capi ahrwwv_sim)

    add_executable(ahr_wwv_wav capi/tools/ahr_wwv_wav.c)
    set_target_properties(ahr_wwv_wav PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(ahr_wwv_wav ahrwwv_capi)

    enable_testing()
    add_test(NAME ahrwwv_tests COMMAND ahrwwv_tests)
    add_test(NAME ahrwwv_capi_tests COMMAND ahrwwv_capi_tests)
    # Fixture: real KL5NE off-air 15 MHz IQ (required for minute-lock regression).
    add_test(NAME palmer_15mhz_real_recording
             COMMAND ${CMAKE_COMMAND}
                     -DDECODER=$<TARGET_FILE:wwv_decode_wav>
                     -DWAV=${CMAKE_CURRENT_SOURCE_DIR}/tests/real/palmer_15mhz_2026-11-10_4k.wav
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/real/check_palmer_15mhz.cmake)
endif()

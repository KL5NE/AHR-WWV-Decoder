cmake_minimum_required(VERSION 3.16)
project(ahrwwv_core CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Portable decoder engine: no Qt, no SDRangel. The SDRangel plugin links this.
add_library(ahrwwv_core STATIC core/src/timecode.cpp core/src/decoder.cpp)
target_include_directories(ahrwwv_core PUBLIC core/include)
set_target_properties(ahrwwv_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

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

    enable_testing()
    add_test(NAME ahrwwv_tests COMMAND ahrwwv_tests)
endif()

# Adds the AHR WWV/WWVH Time Decoder hook to an SDRangel v8.0.0 source tree.
#
#   cmake -DSDRANGEL_SOURCE_DIR=<path to sdrangel> -P apply_integration.cmake
#
# It appends a guarded add_subdirectory() to plugins/channelrx/CMakeLists.txt.
# Nothing else in SDRangel is touched. Running it twice is harmless.
# Afterwards configure SDRangel with -DAHRWWV_PLUGIN_DIR=<AHR repo>/plugin

if(NOT SDRANGEL_SOURCE_DIR)
    message(FATAL_ERROR "Pass -DSDRANGEL_SOURCE_DIR=<path to the SDRangel source tree>")
endif()

set(target "${SDRANGEL_SOURCE_DIR}/plugins/channelrx/CMakeLists.txt")

if(NOT EXISTS "${target}")
    message(FATAL_ERROR "Not an SDRangel source tree: ${target} not found")
endif()

file(READ "${target}" content)

if(content MATCHES "AHRWWV_PLUGIN_DIR")
    message(STATUS "AHR WWV hook already present in ${target}")
    return()
endif()

file(READ "${CMAKE_CURRENT_LIST_DIR}/channelrx-hook.cmake" hook)
file(APPEND "${target}" "\n${hook}")
message(STATUS "AHR WWV hook added to ${target}")

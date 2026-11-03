# AHR WWV/WWVH Time Decoder - SDRangel receive channel plugin.
#
# Built inside an SDRangel v8.0.0 source tree. SDRangel's
# plugins/channelrx/CMakeLists.txt gets one extra line (see
# sdrangel-integration/), which pulls this directory in from the AHR repository:
#
#   add_subdirectory(${AHRWWV_PLUGIN_DIR} ahrwwv)
#
# The portable decoder engine is compiled straight from ../core, unchanged.

project(ahrwwvtimedecoder)

if(SERVER_MODE)
    # Headless SDRangel server build: this plugin is GUI-oriented and has no
    # web API, so it is not built for the server.
    message(STATUS "AHR WWV: not built in server mode")
    return()
endif()

set(AHRWWV_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../core" CACHE PATH "AHR WWV portable decoder core")

if(NOT EXISTS "${AHRWWV_CORE_DIR}/src/decoder.cpp")
    message(FATAL_ERROR "AHR WWV: decoder core not found at ${AHRWWV_CORE_DIR}")
endif()

set(ahrwwv_SOURCES
    ahrwwv.cpp
    ahrwwvsettings.cpp
    ahrwwvbaseband.cpp
    ahrwwvsink.cpp
    ahrwwvplugin.cpp
    ahrwwvgui.cpp
    ahrwwvgui.ui
    ahrwwvwidgets.cpp
    ${AHRWWV_CORE_DIR}/src/decoder.cpp
    ${AHRWWV_CORE_DIR}/src/timecode.cpp
)

set(ahrwwv_HEADERS
    ahrwwv.h
    ahrwwvsettings.h
    ahrwwvbaseband.h
    ahrwwvsink.h
    ahrwwvplugin.h
    ahrwwvreport.h
    ahrwwvgui.h
    ahrwwvwidgets.h
    ${AHRWWV_CORE_DIR}/include/ahrwwv/decoder.h
    ${AHRWWV_CORE_DIR}/include/ahrwwv/timecode.h
)

set(TARGET_NAME ${PLUGINS_PREFIX}ahrwwvtimedecoder)
set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${ahrwwv_SOURCES} ${ahrwwv_HEADERS})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AHRWWVPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${ahrwwv_SOURCES} ${ahrwwv_HEADERS})
endif()

# uic must find ahrwwvwidgets.h (promoted widgets) and the core headers
target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${AHRWWV_CORE_DIR}/include
)

if(WIN32)
    # keep windows.h min/max macros away from std::min/std::max
    target_compile_definitions(${TARGET_NAME} PRIVATE NOMINMAX)
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::Widgets
    sdrbase
    sdrgui
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} OPTIONAL)
endif()

# ------------------------------------------------------------------------------
# Integration tests: real plugin classes + SDRangel sdrbase + simulator.
option(AHRWWV_BUILD_PLUGIN_TESTS "Build the AHR WWV plugin integration tests" OFF)

if(AHRWWV_BUILD_PLUGIN_TESTS)
    set(AHRWWV_SIM_DIR "${AHRWWV_CORE_DIR}/../sim")
    add_executable(ahrwwv_plugin_tests
        tests/test_plugin_integration.cpp
        ahrwwv.cpp
        ahrwwvsettings.cpp
        ahrwwvbaseband.cpp
        ahrwwvsink.cpp
        ${AHRWWV_CORE_DIR}/src/decoder.cpp
        ${AHRWWV_CORE_DIR}/src/timecode.cpp
        ${AHRWWV_SIM_DIR}/wwvsim.cpp
    )
    target_include_directories(ahrwwv_plugin_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${AHRWWV_CORE_DIR}/include
        ${AHRWWV_SIM_DIR}
    )
    target_link_libraries(ahrwwv_plugin_tests PRIVATE Qt::Core sdrbase)
    if(WIN32)
        target_compile_definitions(ahrwwv_plugin_tests PRIVATE NOMINMAX)
        # next to sdrangel.exe / sdrbase.dll (plugins/ redirects runtime output to bin/plugins)
        set_target_properties(ahrwwv_plugin_tests PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${SDRANGEL_BINARY_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${SDRANGEL_BINARY_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${SDRANGEL_BINARY_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${SDRANGEL_BINARY_BIN_DIR}")
    endif()
    set_target_properties(ahrwwv_plugin_tests PROPERTIES AUTOMOC ON)
    add_dependencies(ahrwwv_plugin_tests ${TARGET_NAME})
endif()

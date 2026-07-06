#==============================================================================
# MyPlayer - Professional Windows Media Player
#
# Build system: CMake
# Language:     C++17
# Backends:     FFmpeg (demux/decode), SDL2 (video render + audio output)
#
# Supported containers/codecs (via FFmpeg): MP4, MKV, AVI, MOV,
# MP3, WAV, FLAC, AAC, HEVC (H.265), H.264, AV1.
#
# This file wires together:
#   - Project/target definitions
#   - C++17 standard enforcement
#   - FFmpeg (libavformat/libavcodec/libavutil/libswscale/libswresample)
#   - SDL2 (core + main)
#   - Warnings / sane defaults for MSVC and MinGW
#   - Output directories for a clean Windows deployment layout
#==============================================================================

cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# Project declaration
#------------------------------------------------------------------------------
project(MyPlayer
    VERSION 1.0.0
    DESCRIPTION "Professional Windows Media Player built on FFmpeg + SDL2"
    LANGUAGES CXX
)

#------------------------------------------------------------------------------
# Global build settings
#------------------------------------------------------------------------------

# Enforce C++17 across all targets, no compiler-specific extensions.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to a Release build if the user didn't specify one (single-config
# generators like Makefiles/Ninja; multi-config generators like Visual Studio
# ignore this and use CMAKE_CONFIGURATION_TYPES instead).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Restrict multi-config generators (Visual Studio) to Debug/Release to keep
# the build matrix predictable.
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)
endif()

# Emit compile_commands.json for IDE / clang-tidy tooling.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Unify output locations so all binaries/libs land in predictable folders,
# which simplifies packaging the FFmpeg/SDL2 DLLs alongside the .exe on Windows.
foreach(OUTPUTCONFIG "" "_DEBUG" "_RELEASE" "_RELWITHDEBINFO" "_MINSIZEREL")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/lib)
endforeach()

#------------------------------------------------------------------------------
# Platform check - this project targets Windows
#------------------------------------------------------------------------------
if(NOT WIN32)
    message(WARNING "MyPlayer is designed and tested for Windows targets. "
                     "Building on this platform is unsupported and may fail.")
endif()

#------------------------------------------------------------------------------
# Options
#------------------------------------------------------------------------------
option(MYPLAYER_BUILD_TESTS "Build the MyPlayer test suite" ON)
option(MYPLAYER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

#------------------------------------------------------------------------------
# Third-party dependency: FFmpeg
#
# FFmpeg does not ship first-party CMake config files, so we locate it
# manually. Users are expected to place prebuilt Windows dev binaries
# (e.g. the "shared" or "dev" builds from gyan.dev / BtbN) under
# libs/ffmpeg/{include,lib,bin}, or point FFMPEG_ROOT at an existing install.
#------------------------------------------------------------------------------
set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/libs/ffmpeg" CACHE PATH "Path to FFmpeg dev package root")

find_path(FFMPEG_INCLUDE_DIR
    NAMES libavformat/avformat.h
    PATHS "${FFMPEG_ROOT}/include"
    NO_DEFAULT_PATH
)

set(FFMPEG_REQUIRED_LIBS
    avformat
    avcodec
    avutil
    swscale
    swresample
)

set(FFMPEG_LIBRARIES "")
foreach(LIB_NAME ${FFMPEG_REQUIRED_LIBS})
    find_library(FFMPEG_${LIB_NAME}_LIBRARY
        NAMES ${LIB_NAME}
        PATHS "${FFMPEG_ROOT}/lib"
        NO_DEFAULT_PATH
    )
    if(NOT FFMPEG_${LIB_NAME}_LIBRARY)
        message(FATAL_ERROR
            "Could not find FFmpeg library '${LIB_NAME}' under ${FFMPEG_ROOT}/lib. "
            "Ensure a Windows dev build of FFmpeg (with .lib import libraries) is "
            "extracted into libs/ffmpeg, or set FFMPEG_ROOT to its install location.")
    endif()
    list(APPEND FFMPEG_LIBRARIES ${FFMPEG_${LIB_NAME}_LIBRARY})
endforeach()

if(NOT FFMPEG_INCLUDE_DIR)
    message(FATAL_ERROR
        "Could not find FFmpeg headers (libavformat/avformat.h) under ${FFMPEG_ROOT}/include. "
        "Ensure a Windows dev build of FFmpeg is extracted into libs/ffmpeg.")
endif()

# Imported target wrapping all FFmpeg libraries so downstream targets can
# simply link against MyPlayer::FFmpeg.
add_library(MyPlayer_FFmpeg INTERFACE)
target_include_directories(MyPlayer_FFmpeg INTERFACE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(MyPlayer_FFmpeg INTERFACE ${FFMPEG_LIBRARIES})
add_library(MyPlayer::FFmpeg ALIAS MyPlayer_FFmpeg)

#------------------------------------------------------------------------------
# Third-party dependency: SDL2
#
# Prefer an SDL2 config package if one is installed/vendored (e.g. via
# vcpkg); otherwise fall back to manual discovery under libs/SDL2.
#------------------------------------------------------------------------------
set(SDL2_ROOT "${CMAKE_SOURCE_DIR}/libs/SDL2" CACHE PATH "Path to SDL2 dev package root")

find_package(SDL2 QUIET CONFIG PATHS "${SDL2_ROOT}/cmake" "${SDL2_ROOT}")

if(NOT SDL2_FOUND)
    find_path(SDL2_INCLUDE_DIR
        NAMES SDL.h
        PATHS "${SDL2_ROOT}/include" "${SDL2_ROOT}/include/SDL2"
        NO_DEFAULT_PATH
    )

    # Windows dev packages ship arch-specific import libs (x64/x86).
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_SDL2_LIB_ARCH_DIR "x64")
    else()
        set(_SDL2_LIB_ARCH_DIR "x86")
    endif()

    find_library(SDL2_LIBRARY
        NAMES SDL2
        PATHS "${SDL2_ROOT}/lib/${_SDL2_LIB_ARCH_DIR}" "${SDL2_ROOT}/lib"
        NO_DEFAULT_PATH
    )
    find_library(SDL2MAIN_LIBRARY
        NAMES SDL2main
        PATHS "${SDL2_ROOT}/lib/${_SDL2_LIB_ARCH_DIR}" "${SDL2_ROOT}/lib"
        NO_DEFAULT_PATH
    )

    if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY OR NOT SDL2MAIN_LIBRARY)
        message(FATAL_ERROR
            "Could not find SDL2 development package under ${SDL2_ROOT}. "
            "Download the SDL2 Windows VC development libraries and extract "
            "them into libs/SDL2, or set SDL2_ROOT to an existing install.")
    endif()

    add_library(MyPlayer_SDL2 INTERFACE)
    target_include_directories(MyPlayer_SDL2 INTERFACE ${SDL2_INCLUDE_DIR})
    target_link_libraries(MyPlayer_SDL2 INTERFACE ${SDL2_LIBRARY} ${SDL2MAIN_LIBRARY})
    add_library(MyPlayer::SDL2 ALIAS MyPlayer_SDL2)
else()
    # find_package succeeded (e.g. vcpkg toolchain) - wrap its targets uniformly.
    add_library(MyPlayer_SDL2 INTERFACE)
    target_link_libraries(MyPlayer_SDL2 INTERFACE SDL2::SDL2 SDL2::SDL2main)
    add_library(MyPlayer::SDL2 ALIAS MyPlayer_SDL2)
endif()

#------------------------------------------------------------------------------
# Core application sources
#------------------------------------------------------------------------------
set(MYPLAYER_SOURCES
    src/main.cpp
    src/player.cpp
    src/decoder.cpp
    src/renderer.cpp
    src/audio.cpp
    src/seekbar.cpp
)

set(MYPLAYER_HEADERS
    src/player.h
    src/decoder.h
    src/renderer.h
    src/audio.h
    src/seekbar.h
)

# WIN32 marks this as a GUI subsystem app on Windows (no console window)
# while still working correctly with SDL2's WinMain shim.
add_executable(MyPlayer WIN32
    ${MYPLAYER_SOURCES}
    ${MYPLAYER_HEADERS}
)

target_include_directories(MyPlayer
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(MyPlayer
    PRIVATE
        MyPlayer::FFmpeg
        MyPlayer::SDL2
)

# Windows requires linking against a handful of system libs for FFmpeg's
# networking/bcrypt-based components (safe to include even if unused).
if(WIN32)
    target_link_libraries(MyPlayer PRIVATE ws2_32 secur32 bcrypt)
endif()

#------------------------------------------------------------------------------
# Compiler warnings
#------------------------------------------------------------------------------
if(MSVC)
    target_compile_options(MyPlayer PRIVATE /W4 /permissive-)
    if(MYPLAYER_WARNINGS_AS_ERRORS)
        target_compile_options(MyPlayer PRIVATE /WX)
    endif()
else()
    target_compile_options(MyPlayer PRIVATE -Wall -Wextra -Wpedantic)
    if(MYPLAYER_WARNINGS_AS_ERRORS)
        target_compile_options(MyPlayer PRIVATE -Werror)
    endif()
endif()

#------------------------------------------------------------------------------
# Post-build: copy required runtime DLLs (FFmpeg + SDL2) next to the .exe
# so the app runs directly from the build/bin folder on Windows.
#------------------------------------------------------------------------------
if(WIN32)
    file(GLOB _FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
    if(_FFMPEG_DLLS)
        add_custom_command(TARGET MyPlayer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_FFMPEG_DLLS} $<TARGET_FILE_DIR:MyPlayer>
            COMMENT "Copying FFmpeg runtime DLLs next to MyPlayer.exe"
        )
    endif()

    file(GLOB _SDL2_DLLS "${SDL2_ROOT}/lib/${_SDL2_LIB_ARCH_DIR}/*.dll" "${SDL2_ROOT}/bin/*.dll")
    if(_SDL2_DLLS)
        add_custom_command(TARGET MyPlayer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_SDL2_DLLS} $<TARGET_FILE_DIR:MyPlayer>
            COMMENT "Copying SDL2 runtime DLLs next to MyPlayer.exe"
        )
    endif()
endif()

#------------------------------------------------------------------------------
# Tests subdirectory (optional)
#------------------------------------------------------------------------------
if(MYPLAYER_BUILD_TESTS)
    enable_testing()
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    else()
        message(STATUS "MYPLAYER_BUILD_TESTS is ON but tests/CMakeLists.txt does not exist yet - skipping.")
    endif()
endif()

#------------------------------------------------------------------------------
# Install rules (optional convenience for packaging)
#------------------------------------------------------------------------------
install(TARGETS MyPlayer RUNTIME DESTINATION bin)
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    install(DIRECTORY assets/ DESTINATION bin/assets)
endif()

#------------------------------------------------------------------------------
# Summary
#------------------------------------------------------------------------------
message(STATUS "==============================================")
message(STATUS " MyPlayer configuration summary")
message(STATUS "  C++ standard   : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type     : ${CMAKE_BUILD_TYPE}")
message(STATUS "  FFmpeg include : ${FFMPEG_INCLUDE_DIR}")
message(STATUS "  FFmpeg libs    : ${FFMPEG_LIBRARIES}")
message(STATUS "  SDL2 found via : ${SDL2_FOUND}")
message(STATUS "  Tests enabled  : ${MYPLAYER_BUILD_TESTS}")
message(STATUS "==============================================")
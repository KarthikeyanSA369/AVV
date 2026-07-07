# ============================================================================
# CMakeLists.txt — AV Player
# ============================================================================
cmake_minimum_required(VERSION 3.21)

project(AVPlayer LANGUAGES CXX)

# ----------------------------------------------------------------------------
# Language standard
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ----------------------------------------------------------------------------
# vcpkg sanity check
# ----------------------------------------------------------------------------
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "CMAKE_TOOLCHAIN_FILE is not set. Configure with:\n"
        "  -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake")
endif()

# ----------------------------------------------------------------------------
# SDL2 (vcpkg CONFIG package)
# ----------------------------------------------------------------------------
find_package(SDL2 CONFIG REQUIRED)

# ----------------------------------------------------------------------------
# FFmpeg (vcpkg ships pkg-config .pc files, no CMake config package)
# ----------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_AVP_VCPKG_TRIPLET_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
elseif(DEFINED _VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_AVP_VCPKG_TRIPLET_DIR "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
else()
    message(FATAL_ERROR "Unable to resolve vcpkg installed directory. Ensure vcpkg toolchain file is used.")
endif()

find_program(PKG_CONFIG_EXECUTABLE
    NAMES pkgconf pkg-config
    PATHS
        "${_AVP_VCPKG_TRIPLET_DIR}/tools/pkgconf"
        "${_AVP_VCPKG_TRIPLET_DIR}/tools/pkgconf/bin"
    NO_DEFAULT_PATH
)

if(NOT PKG_CONFIG_EXECUTABLE)
    find_program(PKG_CONFIG_EXECUTABLE NAMES pkgconf pkg-config)
endif()

if(NOT PKG_CONFIG_EXECUTABLE)
    message(FATAL_ERROR "pkgconf/pkg-config executable not found. Add 'pkgconf' as a host dependency in vcpkg.json.")
endif()

set(ENV{PKG_CONFIG_PATH}
    "${_AVP_VCPKG_TRIPLET_DIR}/lib/pkgconfig$<SEMICOLON>${_AVP_VCPKG_TRIPLET_DIR}/debug/lib/pkgconfig"
)
set(ENV{PKG_CONFIG_PATH} "${_AVP_VCPKG_TRIPLET_DIR}/lib/pkgconfig;${_AVP_VCPKG_TRIPLET_DIR}/debug/lib/pkgconfig")

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libavdevice
    libavfilter
    libswscale
    libswresample
)

# ----------------------------------------------------------------------------
# Sources
# ----------------------------------------------------------------------------
set(AVPLAYER_SOURCES
    src/main.cpp
    src/player.cpp
    src/decoder.cpp
    src/renderer.cpp
    src/audio.cpp
    src/seekbar.cpp
)

set(AVPLAYER_HEADERS
    src/player.h
    src/decoder.h
    src/renderer.h
    src/audio.h
    src/seekbar.h
)

add_executable(AVPlayer WIN32
    ${AVPLAYER_SOURCES}
    ${AVPLAYER_HEADERS}
)

target_include_directories(AVPlayer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(AVPlayer PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(AVPlayer PRIVATE /W4 /utf-8)
endif()

target_link_libraries(AVPlayer PRIVATE
    SDL2::SDL2main
    SDL2::SDL2
    PkgConfig::FFMPEG
)

set_target_properties(AVPlayer PROPERTIES
    OUTPUT_NAME "AVPlayer"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

# ----------------------------------------------------------------------------
# Copy runtime DLLs (SDL2 + FFmpeg) next to the executable
# ----------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET AVPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:AVPlayer>
        COMMAND_EXPAND_LISTS
    )

    file(GLOB _AVP_FFMPEG_DLLS
        "${_AVP_VCPKG_TRIPLET_DIR}/bin/av*.dll"
        "${_AVP_VCPKG_TRIPLET_DIR}/bin/sw*.dll"
    )

    foreach(_dll ${_AVP_FFMPEG_DLLS})
        add_custom_command(TARGET AVPlayer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_dll}"
                "$<TARGET_FILE_DIR:AVPlayer>"
        )
    endforeach()
endif()

# ----------------------------------------------------------------------------
# Copy assets
# ----------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET AVPlayer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:AVPlayer>/assets"
    )
endif()

install(TARGETS AVPlayer RUNTIME DESTINATION bin)

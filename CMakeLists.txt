cmake_minimum_required(VERSION 3.22)

project(AVPlayer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -O2)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# -----------------------------
# vcpkg support
# -----------------------------
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    message(STATUS "Using vcpkg toolchain")
endif()

find_package(SDL2 CONFIG REQUIRED)

find_package(PkgConfig REQUIRED)

find_path(
    FFMPEG_INCLUDE_DIR
    libavcodec/avcodec.h
)

find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(SWSCALE_LIBRARY swscale)
find_library(SWRESAMPLE_LIBRARY swresample)

include_directories(
    src
    include
    ${FFMPEG_INCLUDE_DIR}
)

set(SOURCES

    src/main.cpp

    src/player.cpp

    src/decoder.cpp

    src/renderer.cpp

    src/audio.cpp

    src/seekbar.cpp

)

set(HEADERS

    src/player.h

    src/decoder.h

    src/renderer.h

    src/audio.h

    src/seekbar.h

)

add_executable(AVPlayer
    ${SOURCES}
    ${HEADERS}
)

# -------------------------------------------------------
# Link Libraries
# -------------------------------------------------------

target_link_libraries(AVPlayer PRIVATE

    SDL2::SDL2
    SDL2::SDL2main

    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}

)

# -------------------------------------------------------
# Windows System Libraries
# -------------------------------------------------------

if(WIN32)

target_link_libraries(AVPlayer PRIVATE

    ws2_32
    bcrypt
    secur32
    winmm
    version
    imm32
    setupapi
)

endif()

# -------------------------------------------------------
# MSVC Options
# -------------------------------------------------------

if(MSVC)

target_compile_definitions(AVPlayer PRIVATE

    _CRT_SECURE_NO_WARNINGS
    NOMINMAX

)

endif()

# -------------------------------------------------------
# Install
# -------------------------------------------------------

install(TARGETS AVPlayer
        RUNTIME DESTINATION bin)

# -------------------------------------------------------
# Copy FFmpeg DLLs
# -------------------------------------------------------

if(WIN32)

file(GLOB FFMPEG_DLLS

    "${CMAKE_SOURCE_DIR}/libs/ffmpeg/bin/*.dll"

)

foreach(DLL ${FFMPEG_DLLS})

add_custom_command(TARGET AVPlayer POST_BUILD

COMMAND ${CMAKE_COMMAND} -E copy_if_different

${DLL}

$<TARGET_FILE_DIR:AVPlayer>

)

endforeach()

endif()

# -------------------------------------------------------
# Copy SDL2 DLL
# -------------------------------------------------------

if(WIN32)

file(GLOB SDL_DLL

"${CMAKE_SOURCE_DIR}/libs/SDL2/lib/x64/*.dll"

"${CMAKE_SOURCE_DIR}/libs/SDL2/bin/*.dll"

)

foreach(DLL ${SDL_DLL})

add_custom_command(TARGET AVPlayer POST_BUILD

COMMAND ${CMAKE_COMMAND} -E copy_if_different

${DLL}

$<TARGET_FILE_DIR:AVPlayer>

)

endforeach()

endif()

# -------------------------------------------------------
# Build Summary
# -------------------------------------------------------

message(STATUS "===================================")
message(STATUS " AV Player")
message(STATUS " Version : ${PROJECT_VERSION}")
message(STATUS " C++     : ${CMAKE_CXX_STANDARD}")
message(STATUS "===================================")

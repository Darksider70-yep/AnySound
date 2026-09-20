cmake_minimum_required(VERSION 3.22.1)

project(chorus_native LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Root directory of AnySound repository
set(CHORUS_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

# Include FetchContent for Opus, miniaudio, nlohmann_json
include(FetchContent)

# 1. libopus
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)

find_package(Opus CONFIG QUIET)
if(NOT Opus_FOUND)
    FetchContent_Declare(
        opus
        GIT_REPOSITORY https://github.com/xiph/opus.git
        GIT_TAG        v1.5.2
    )
    FetchContent_MakeAvailable(opus)
endif()

# 2. miniaudio
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.21
)
FetchContent_MakeAvailable(miniaudio)

# 3. nlohmann_json
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Compiler flags
add_library(chorus_compiler_flags INTERFACE)
target_compile_options(chorus_compiler_flags INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
)

# Core library source files
add_library(chorus_core STATIC
    ${CHORUS_ROOT_DIR}/core/src/core.cpp
    ${CHORUS_ROOT_DIR}/core/src/codec/opus_codec.cpp
    ${CHORUS_ROOT_DIR}/core/src/net/udp_socket.cpp
    ${CHORUS_ROOT_DIR}/core/src/platform/audio_device.cpp
    ${CHORUS_ROOT_DIR}/core/src/proto/packet.cpp
    ${CHORUS_ROOT_DIR}/core/src/sync/clock_estimator.cpp
    ${CHORUS_ROOT_DIR}/core/src/sync/jitter_buffer.cpp
    ${CHORUS_ROOT_DIR}/core/src/sync/resampler.cpp
    ${CHORUS_ROOT_DIR}/core/src/sync/drift_controller.cpp
    ${CHORUS_ROOT_DIR}/core/src/playback/timeline_buffer.cpp
    ${CHORUS_ROOT_DIR}/core/src/net/control_message.cpp
    ${CHORUS_ROOT_DIR}/core/src/net/tcp_socket.cpp
    ${CHORUS_ROOT_DIR}/core/src/net/discovery.cpp
    ${CHORUS_ROOT_DIR}/core/src/session/pin_authenticator.cpp
    ${CHORUS_ROOT_DIR}/core/src/session/host_session.cpp
    ${CHORUS_ROOT_DIR}/core/src/session/client_session.cpp
    ${CHORUS_ROOT_DIR}/core/src/app/app_controller.cpp
    ${CHORUS_ROOT_DIR}/core/src/net/crypto_channel.cpp
    ${CHORUS_ROOT_DIR}/core/src/sync/latency_tuner.cpp
    ${CHORUS_ROOT_DIR}/core/src/diagnostic/diagnostic_logger.cpp
    ${CHORUS_ROOT_DIR}/core/src/playback/delayed_host_renderer.cpp
)

target_include_directories(chorus_core
    PUBLIC
        ${CHORUS_ROOT_DIR}/core/include
    PRIVATE
        ${CHORUS_ROOT_DIR}/core/src
    SYSTEM PRIVATE
        ${miniaudio_SOURCE_DIR}
)

target_link_libraries(chorus_core
    PUBLIC
        chorus_compiler_flags
        nlohmann_json::nlohmann_json
    PRIVATE
        Opus::opus
)

# Shared JNI library
add_library(chorus_jni SHARED
    src/main/cpp/chorus_jni.cpp
)

target_include_directories(chorus_jni
    PRIVATE
        ${CHORUS_ROOT_DIR}/core/include
)

find_library(log-lib log)
find_library(android-lib android)
find_library(aaudio-lib aaudio)
find_library(opensles-lib OpenSLES)

target_link_libraries(chorus_jni
    PRIVATE
        chorus_core
        ${log-lib}
        ${android-lib}
        ${aaudio-lib}
        ${opensles-lib}
)

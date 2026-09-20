add_library(chorus_core STATIC
    src/core.cpp
    src/codec/opus_codec.cpp
    src/net/udp_socket.cpp
    src/platform/audio_device.cpp
)

target_include_directories(chorus_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    SYSTEM PRIVATE
        ${miniaudio_SOURCE_DIR}
)

target_link_libraries(chorus_core
    PUBLIC
        chorus_compiler_flags
    PRIVATE
        Opus::opus
)

if(WIN32)
    target_link_libraries(chorus_core
        PUBLIC
            ws2_32
    )
endif()

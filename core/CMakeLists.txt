add_library(chorus_core STATIC
    src/core.cpp
    src/codec/opus_codec.cpp
    src/net/udp_socket.cpp
    src/platform/audio_device.cpp
    src/proto/packet.cpp
    src/sync/clock_estimator.cpp
    src/sync/jitter_buffer.cpp
    src/sync/resampler.cpp
    src/sync/drift_controller.cpp
    src/playback/timeline_buffer.cpp
    src/net/control_message.cpp
    src/net/tcp_socket.cpp
    src/net/discovery.cpp
    src/session/pin_authenticator.cpp
    src/session/host_session.cpp
    src/session/client_session.cpp
    src/app/app_controller.cpp
    src/net/crypto_channel.cpp
    src/sync/latency_tuner.cpp
    src/diagnostic/diagnostic_logger.cpp
    src/playback/delayed_host_renderer.cpp
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
        nlohmann_json::nlohmann_json
    PRIVATE
        Opus::opus
)

if(WIN32)
    target_link_libraries(chorus_core
        PUBLIC
            ws2_32
    )
endif()

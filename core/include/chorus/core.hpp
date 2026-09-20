#pragma once

#include <cstdint>
#include <string_view>

namespace chorus {

/// @brief Protocol version as specified in architecture.md section 8.
inline constexpr uint8_t kProtocolVersion = 1;

/// @brief Default TCP port for control messages.
inline constexpr uint16_t kDefaultTcpControlPort = 47800;

/// @brief Default UDP port for audio and clock synchronization.
inline constexpr uint16_t kDefaultUdpDataPort = 47801;

/// @brief Default playout target latency in milliseconds.
inline constexpr uint64_t kDefaultTargetLatencyMs = 300;

/// @brief Returns the version string of the Chorus core library.
[[nodiscard]] std::string_view version() noexcept;

}  // namespace chorus

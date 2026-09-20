#pragma once

#include <chorus/codec/opus_codec.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace chorus {

/// @brief Fractional stereo audio resampler with continuous phase preservation across frame boundaries.
/// Safe for hard real-time audio paths (zero allocations during process()).
class Resampler {
public:
    Resampler();
    ~Resampler() = default;

    Resampler(const Resampler&) = delete;
    Resampler& operator=(const Resampler&) = delete;
    Resampler(Resampler&&) noexcept = default;
    Resampler& operator=(Resampler&&) noexcept = default;

    /// @brief Sets resampling ratio (output_rate / input_rate = 1.0 + ppm/1e6).
    /// @param ratio Target ratio (e.g. 0.9998 to 1.0002 for +/- 200 ppm).
    void set_ratio(double ratio) noexcept;

    /// @brief Returns current resampling ratio.
    [[nodiscard]] double ratio() const noexcept;

    /// @brief Resamples an input frame of stereo floats into an output buffer.
    /// @param input Interleaved stereo float samples.
    /// @param output Buffer to receive resampled interleaved stereo floats.
    /// @return Number of stereo sample frames (pairs of floats) written to output.
    size_t process(std::span<const float> input, std::span<float> output) noexcept;

    /// @brief Resets phase and state history.
    void reset() noexcept;

private:
    double ratio_{1.0};
    double phase_{0.0};  // Sub-sample phase offset [0.0, 1.0)
    std::array<float, 4> prev_samples_{0.0F, 0.0F, 0.0F, 0.0F};  // Previous 2 stereo frames for cubic interpolation
    bool has_prev_{false};
};

}  // namespace chorus

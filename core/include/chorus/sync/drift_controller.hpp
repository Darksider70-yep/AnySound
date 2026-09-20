#pragma once

#include <cstdint>

namespace chorus {

inline constexpr double kDefaultMaxCorrectionPpm = 200.0;
inline constexpr int64_t kDefaultHardResyncThresholdUs = 30000;
inline constexpr double kDefaultKp = 0.02;
inline constexpr double kDefaultKi = 0.002;

/// @brief PI-based clock drift and phase error controller.
/// Drives the Resampler to eliminate residual phase error without exceeding +/-200 ppm.
/// Triggers a hard resync if phase error exceeds 30 ms.
class DriftController {
public:
    explicit DriftController(double max_correction_ppm = kDefaultMaxCorrectionPpm,
                             int64_t hard_resync_threshold_us = kDefaultHardResyncThresholdUs,
                             double prop_gain = kDefaultKp,
                             double integral_gain = kDefaultKi);
    ~DriftController() = default;

    DriftController(const DriftController&) = default;
    DriftController& operator=(const DriftController&) = default;
    DriftController(DriftController&&) noexcept = default;
    DriftController& operator=(DriftController&&) noexcept = default;

    /// @brief Updates controller with current phase error and clock skew estimate.
    /// @param phase_error_us (Scheduled playout time - Actual playhead time) in microseconds.
    ///                       Positive means audio is playing too slow / behind, needs speedup.
    /// @param skew_ppm Estimated static clock skew from ClockEstimator in ppm.
    /// @param dt_sec Time elapsed since last update in seconds.
    void update(int64_t phase_error_us, double skew_ppm, double dt_sec) noexcept;

    /// @brief Returns the computed total resampling ratio (1.0 + total_ppm / 1e6).
    [[nodiscard]] double resampler_ratio() const noexcept;

    /// @brief Returns the computed dynamic PI correction in ppm.
    [[nodiscard]] double dynamic_correction_ppm() const noexcept;

    /// @brief Returns the total applied correction in ppm (skew_ppm + dynamic_ppm).
    [[nodiscard]] double total_correction_ppm() const noexcept;

    /// @brief Returns true if phase error exceeded 30 ms, requiring a hard buffer flush/re-anchor.
    [[nodiscard]] bool needs_hard_resync() const noexcept;

    /// @brief Acknowledges and clears the hard resync flag.
    void clear_resync() noexcept;

    /// @brief Resets integrator and state.
    void reset() noexcept;

private:
    double max_correction_ppm_{kDefaultMaxCorrectionPpm};
    int64_t hard_resync_threshold_us_{kDefaultHardResyncThresholdUs};
    double kp_{kDefaultKp};
    double ki_{kDefaultKi};

    double integral_error_us_{0.0};
    double dynamic_correction_ppm_{0.0};
    double total_correction_ppm_{0.0};
    double resampler_ratio_{1.0};
    bool needs_hard_resync_{false};
};

}  // namespace chorus

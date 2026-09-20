#include <chorus/sync/drift_controller.hpp>

#include <algorithm>
#include <cmath>

namespace chorus {

namespace {

constexpr double kMaxDtSec = 5.0;
constexpr double kPpmScaling = 1000000.0;

}  // namespace

DriftController::DriftController(double max_correction_ppm,
                                 int64_t hard_resync_threshold_us,
                                 double prop_gain,
                                 double integral_gain)
    : max_correction_ppm_(max_correction_ppm),
      hard_resync_threshold_us_(hard_resync_threshold_us),
      kp_(prop_gain),
      ki_(integral_gain) {}

void DriftController::update(int64_t phase_error_us, double skew_ppm, double dt_sec) noexcept {
    if (std::abs(phase_error_us) > hard_resync_threshold_us_) {
        needs_hard_resync_ = true;
        reset();
        return;
    }

    if (dt_sec > 0.0 && dt_sec < kMaxDtSec) {
        integral_error_us_ += static_cast<double>(phase_error_us) * dt_sec;
        // Anti-windup clamping on integral term
        const double max_integral = max_correction_ppm_ / std::max(ki_, 1e-6);
        integral_error_us_ = std::clamp(integral_error_us_, -max_integral, max_integral);
    }

    const double p_term = kp_ * static_cast<double>(phase_error_us);
    const double i_term = ki_ * integral_error_us_;
    dynamic_correction_ppm_ = std::clamp(p_term + i_term, -max_correction_ppm_, max_correction_ppm_);

    // Total correction = static clock skew + dynamic phase correction, clamped to max allowed
    total_correction_ppm_ = std::clamp(skew_ppm + dynamic_correction_ppm_, -max_correction_ppm_, max_correction_ppm_);
    resampler_ratio_ = 1.0 + (total_correction_ppm_ / kPpmScaling);
}

double DriftController::resampler_ratio() const noexcept {
    return resampler_ratio_;
}

double DriftController::dynamic_correction_ppm() const noexcept {
    return dynamic_correction_ppm_;
}

double DriftController::total_correction_ppm() const noexcept {
    return total_correction_ppm_;
}

bool DriftController::needs_hard_resync() const noexcept {
    return needs_hard_resync_;
}

void DriftController::clear_resync() noexcept {
    needs_hard_resync_ = false;
}

void DriftController::reset() noexcept {
    integral_error_us_ = 0.0;
    dynamic_correction_ppm_ = 0.0;
    total_correction_ppm_ = 0.0;
    resampler_ratio_ = 1.0;
}

}  // namespace chorus

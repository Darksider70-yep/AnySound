#include <chorus/sync/resampler.hpp>

#include <algorithm>
#include <cmath>

namespace chorus {

namespace {

inline float interpolate_cubic(float y0, float y1, float y2, float y3, float t) noexcept {
    const float a0 = -0.5F * y0 + 1.5F * y1 - 1.5F * y2 + 0.5F * y3;
    const float a1 = y0 - 2.5F * y1 + 2.0F * y2 - 0.5F * y3;
    const float a2 = -0.5F * y0 + 0.5F * y2;
    const float a3 = y1;
    return ((a0 * t + a1) * t + a2) * t + a3;
}

}  // namespace

Resampler::Resampler() = default;

void Resampler::set_ratio(double ratio) noexcept {
    // Clamp to reasonable limits [0.9990, 1.0010] (+/- 1000 ppm)
    ratio_ = std::clamp(ratio, 0.9990, 1.0010);
}

double Resampler::ratio() const noexcept {
    return ratio_;
}

size_t Resampler::process(std::span<const float> input, std::span<float> output) noexcept {
    if (input.empty() || output.empty()) {
        return 0;
    }

    const size_t in_frames = input.size() / static_cast<size_t>(kChannels);
    const size_t max_out_frames = output.size() / static_cast<size_t>(kChannels);

    if (in_frames == 0 || max_out_frames == 0) {
        return 0;
    }

    // Direct 1:1 bypass optimization if ratio is practically 1.0 and phase is 0
    if (std::abs(ratio_ - 1.0) < 1e-7 && phase_ == 0.0) {
        const size_t frames_to_copy = std::min(in_frames, max_out_frames);
        const size_t floats_to_copy = frames_to_copy * static_cast<size_t>(kChannels);
        std::ranges::copy(input.subspan(0, floats_to_copy), output.begin());

        if (frames_to_copy >= 2) {
            prev_samples_[0] = input[(frames_to_copy - 2) * 2];
            prev_samples_[1] = input[(frames_to_copy - 2) * 2 + 1];
            prev_samples_[2] = input[(frames_to_copy - 1) * 2];
            prev_samples_[3] = input[(frames_to_copy - 1) * 2 + 1];
            has_prev_ = true;
        }
        return frames_to_copy;
    }

    size_t out_frame_idx = 0;
    const double step = 1.0 / ratio_;

    while (out_frame_idx < max_out_frames) {
        const auto in_idx_floor = static_cast<int64_t>(std::floor(phase_));
        if (in_idx_floor >= static_cast<int64_t>(in_frames)) {
            break;
        }

        const auto frac_t = static_cast<float>(phase_ - static_cast<double>(in_idx_floor));

        // Sample retrieval for 4-point cubic interpolation across L and R
        float l0 = 0.0F;
        float l1 = 0.0F;
        float l2 = 0.0F;
        float l3 = 0.0F;
        float r0 = 0.0F;
        float r1 = 0.0F;
        float r2 = 0.0F;
        float r3 = 0.0F;

        for (int ch = 0; ch < 2; ++ch) {
            auto get_sample = [&](int64_t idx) -> float {
                if (idx < 0) {
                    if (has_prev_) {
                        if (idx == -1) {
                            return prev_samples_[2 + ch];
                        }
                        if (idx == -2) {
                            return prev_samples_[ch];
                        }
                    }
                    return input[static_cast<size_t>(ch)];
                }
                if (idx >= static_cast<int64_t>(in_frames)) {
                    return input[(in_frames - 1) * 2 + static_cast<size_t>(ch)];
                }
                return input[static_cast<size_t>(idx) * 2 + static_cast<size_t>(ch)];
            };

            if (ch == 0) {
                l0 = get_sample(in_idx_floor - 1);
                l1 = get_sample(in_idx_floor);
                l2 = get_sample(in_idx_floor + 1);
                l3 = get_sample(in_idx_floor + 2);
            } else {
                r0 = get_sample(in_idx_floor - 1);
                r1 = get_sample(in_idx_floor);
                r2 = get_sample(in_idx_floor + 1);
                r3 = get_sample(in_idx_floor + 2);
            }
        }

        output[out_frame_idx * 2] = interpolate_cubic(l0, l1, l2, l3, frac_t);
        output[out_frame_idx * 2 + 1] = interpolate_cubic(r0, r1, r2, r3, frac_t);
        out_frame_idx++;

        phase_ += step;
    }

    // Preserve previous frames for next block continuity
    if (in_frames >= 2) {
        prev_samples_[0] = input[(in_frames - 2) * 2];
        prev_samples_[1] = input[(in_frames - 2) * 2 + 1];
        prev_samples_[2] = input[(in_frames - 1) * 2];
        prev_samples_[3] = input[(in_frames - 1) * 2 + 1];
        has_prev_ = true;
    }

    phase_ -= static_cast<double>(in_frames);
    return out_frame_idx;
}

void Resampler::reset() noexcept {
    ratio_ = 1.0;
    phase_ = 0.0;
    prev_samples_[0] = 0.0F;
    prev_samples_[1] = 0.0F;
    prev_samples_[2] = 0.0F;
    prev_samples_[3] = 0.0F;
    has_prev_ = false;
}

}  // namespace chorus

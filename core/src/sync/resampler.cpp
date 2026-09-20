#include <chorus/sync/resampler.hpp>

#include <algorithm>
#include <cmath>

namespace chorus {

namespace {

constexpr double kMinResampleRatio = 0.9990;
constexpr double kMaxResampleRatio = 1.0010;
constexpr double kPhaseEpsilon = 1e-7;

inline float interpolate_cubic(float sample_0, float sample_1, float sample_2, float sample_3, float fraction_t) noexcept {
    const float coeff_0 = (-0.5F * sample_0) + (1.5F * sample_1) - (1.5F * sample_2) + (0.5F * sample_3);
    const float coeff_1 = sample_0 - (2.5F * sample_1) + (2.0F * sample_2) - (0.5F * sample_3);
    const float coeff_2 = (-0.5F * sample_0) + (0.5F * sample_2);
    const float coeff_3 = sample_1;
    return (((((coeff_0 * fraction_t) + coeff_1) * fraction_t) + coeff_2) * fraction_t) + coeff_3;
}

float fetch_channel_sample(int64_t sample_idx,
                           size_t channel,
                           size_t in_frames,
                           bool has_prev,
                           const std::array<float, 4>& prev_samples,
                           std::span<const float> input) noexcept {
    if (sample_idx < 0) {
        if (has_prev) {
            if (sample_idx == -1) {
                return (channel == 0) ? prev_samples[2] : prev_samples[3];
            }
            if (sample_idx == -2) {
                return (channel == 0) ? prev_samples[0] : prev_samples[1];
            }
        }
        return input[channel];
    }
    if (sample_idx >= static_cast<int64_t>(in_frames)) {
        return input[((in_frames - 1) * 2) + channel];
    }
    return input[(static_cast<size_t>(sample_idx) * 2) + channel];
}

}  // namespace

Resampler::Resampler() = default;

void Resampler::set_ratio(double ratio) noexcept {
    // Clamp to reasonable limits [0.9990, 1.0010] (+/- 1000 ppm)
    ratio_ = std::clamp(ratio, kMinResampleRatio, kMaxResampleRatio);
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
    if (std::abs(ratio_ - 1.0) < kPhaseEpsilon && phase_ == 0.0) {
        const size_t frames_to_copy = std::min(in_frames, max_out_frames);
        const size_t floats_to_copy = frames_to_copy * static_cast<size_t>(kChannels);
        std::ranges::copy(input.subspan(0, floats_to_copy), output.begin());

        if (frames_to_copy >= 2) {
            prev_samples_[0] = input[((frames_to_copy - 2) * 2)];
            prev_samples_[1] = input[((frames_to_copy - 2) * 2) + 1];
            prev_samples_[2] = input[((frames_to_copy - 1) * 2)];
            prev_samples_[3] = input[((frames_to_copy - 1) * 2) + 1];
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
        const float left_0 = fetch_channel_sample(in_idx_floor - 1, 0, in_frames, has_prev_, prev_samples_, input);
        const float left_1 = fetch_channel_sample(in_idx_floor, 0, in_frames, has_prev_, prev_samples_, input);
        const float left_2 = fetch_channel_sample(in_idx_floor + 1, 0, in_frames, has_prev_, prev_samples_, input);
        const float left_3 = fetch_channel_sample(in_idx_floor + 2, 0, in_frames, has_prev_, prev_samples_, input);

        const float right_0 = fetch_channel_sample(in_idx_floor - 1, 1, in_frames, has_prev_, prev_samples_, input);
        const float right_1 = fetch_channel_sample(in_idx_floor, 1, in_frames, has_prev_, prev_samples_, input);
        const float right_2 = fetch_channel_sample(in_idx_floor + 1, 1, in_frames, has_prev_, prev_samples_, input);
        const float right_3 = fetch_channel_sample(in_idx_floor + 2, 1, in_frames, has_prev_, prev_samples_, input);

        output[out_frame_idx * 2] = interpolate_cubic(left_0, left_1, left_2, left_3, frac_t);
        output[(out_frame_idx * 2) + 1] = interpolate_cubic(right_0, right_1, right_2, right_3, frac_t);
        out_frame_idx++;

        phase_ += step;
    }

    // Preserve previous frames for next block continuity
    if (in_frames >= 2) {
        prev_samples_[0] = input[((in_frames - 2) * 2)];
        prev_samples_[1] = input[((in_frames - 2) * 2) + 1];
        prev_samples_[2] = input[((in_frames - 1) * 2)];
        prev_samples_[3] = input[((in_frames - 1) * 2) + 1];
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

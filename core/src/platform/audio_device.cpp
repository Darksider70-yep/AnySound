#include <chorus/platform/audio_device.hpp>

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wconversion"
    #pragma GCC diagnostic ignored "-Wsign-conversion"
    #pragma GCC diagnostic ignored "-Wsign-compare"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wshadow"
#endif

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#include <miniaudio.h>

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <iostream>

namespace chorus {

// -----------------------------------------------------------------------------
// AudioCaptureDevice
// -----------------------------------------------------------------------------

struct CaptureDeviceState {
    ma_device device{};
    ma_context context{};
    SpscRing<float>* ring{nullptr};
    std::atomic<bool> running{false};
    std::atomic<uint64_t> frames_captured{0};
    bool context_initialized{false};
    bool device_initialized{false};
};

struct AudioCaptureDevice::Impl {
    CaptureDeviceState state{};
};

namespace {

void capture_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pOutput;
    auto* state = static_cast<CaptureDeviceState*>(pDevice->pUserData);
    if (state == nullptr || state->ring == nullptr || pInput == nullptr) {
        return;
    }

    const auto* samples = static_cast<const float*>(pInput);
    const size_t total_floats = static_cast<size_t>(frameCount) * kChannels;

    state->ring->write(std::span<const float>(samples, total_floats));
    state->frames_captured.fetch_add(frameCount, std::memory_order_relaxed);
}

}  // namespace

AudioCaptureDevice::AudioCaptureDevice() : impl_(std::make_unique<Impl>()) {}

AudioCaptureDevice::~AudioCaptureDevice() {
    stop();
}

bool AudioCaptureDevice::start_loopback(SpscRing<float>* ring) {
    if (impl_->state.running.load()) {
        return true;
    }
    impl_->state.ring = ring;

    ma_result result = ma_context_init(nullptr, 0, nullptr, &impl_->state.context);
    if (result != MA_SUCCESS) {
        return false;
    }
    impl_->state.context_initialized = true;

    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.capture.format = ma_format_f32;
    config.capture.channels = kChannels;
    config.sampleRate = kSampleRate;
    config.dataCallback = capture_data_callback;
    config.pUserData = &impl_->state;

    result = ma_device_init(&impl_->state.context, &config, &impl_->state.device);
    if (result != MA_SUCCESS) {
        // Fallback to standard capture device if loopback is not supported on this platform/OS
        config = ma_device_config_init(ma_device_type_capture);
        config.capture.format = ma_format_f32;
        config.capture.channels = kChannels;
        config.sampleRate = kSampleRate;
        config.dataCallback = capture_data_callback;
        config.pUserData = &impl_->state;

        result = ma_device_init(&impl_->state.context, &config, &impl_->state.device);
        if (result != MA_SUCCESS) {
            ma_context_uninit(&impl_->state.context);
            impl_->state.context_initialized = false;
            return false;
        }
    }
    impl_->state.device_initialized = true;

    result = ma_device_start(&impl_->state.device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&impl_->state.device);
        ma_context_uninit(&impl_->state.context);
        impl_->state.device_initialized = false;
        impl_->state.context_initialized = false;
        return false;
    }

    impl_->state.running.store(true);
    return true;
}

void AudioCaptureDevice::stop() {
    if (impl_->state.running.load()) {
        impl_->state.running.store(false);
        if (impl_->state.device_initialized) {
            ma_device_stop(&impl_->state.device);
            ma_device_uninit(&impl_->state.device);
            impl_->state.device_initialized = false;
        }
        if (impl_->state.context_initialized) {
            ma_context_uninit(&impl_->state.context);
            impl_->state.context_initialized = false;
        }
    }
}

bool AudioCaptureDevice::is_running() const noexcept {
    return impl_->state.running.load();
}

uint64_t AudioCaptureDevice::frames_captured() const noexcept {
    return impl_->state.frames_captured.load();
}

// -----------------------------------------------------------------------------
// AudioPlaybackDevice
// -----------------------------------------------------------------------------

struct PlaybackDeviceState {
    ma_device device{};
    SpscRing<float>* ring{nullptr};
    std::atomic<bool> running{false};
    std::atomic<uint64_t> frames_rendered{0};
    std::atomic<uint64_t> underruns{0};
    bool device_initialized{false};
};

struct AudioPlaybackDevice::Impl {
    PlaybackDeviceState state{};
};

namespace {

void playback_data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput;
    auto* state = static_cast<PlaybackDeviceState*>(pDevice->pUserData);
    if (state == nullptr || pOutput == nullptr) {
        return;
    }

    auto* out_samples = static_cast<float*>(pOutput);
    const size_t needed_floats = static_cast<size_t>(frameCount) * kChannels;

    if (state->ring == nullptr) {
        std::fill(out_samples, out_samples + needed_floats, 0.0f);
        return;
    }

    const size_t read_floats = state->ring->read(std::span<float>(out_samples, needed_floats));
    if (read_floats < needed_floats) {
        std::fill(out_samples + read_floats, out_samples + needed_floats, 0.0f);
        state->underruns.fetch_add(1, std::memory_order_relaxed);
    }

    state->frames_rendered.fetch_add(frameCount, std::memory_order_relaxed);
}

}  // namespace

AudioPlaybackDevice::AudioPlaybackDevice() : impl_(std::make_unique<Impl>()) {}

AudioPlaybackDevice::~AudioPlaybackDevice() {
    stop();
}

bool AudioPlaybackDevice::start_playback(SpscRing<float>* ring) {
    if (impl_->state.running.load()) {
        return true;
    }
    impl_->state.ring = ring;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = kChannels;
    config.sampleRate = kSampleRate;
    config.dataCallback = playback_data_callback;
    config.pUserData = &impl_->state;

    ma_result result = ma_device_init(nullptr, &config, &impl_->state.device);
    if (result != MA_SUCCESS) {
        return false;
    }
    impl_->state.device_initialized = true;

    result = ma_device_start(&impl_->state.device);
    if (result != MA_SUCCESS) {
        ma_device_uninit(&impl_->state.device);
        impl_->state.device_initialized = false;
        return false;
    }

    impl_->state.running.store(true);
    return true;
}

void AudioPlaybackDevice::stop() {
    if (impl_->state.running.load()) {
        impl_->state.running.store(false);
        if (impl_->state.device_initialized) {
            ma_device_stop(&impl_->state.device);
            ma_device_uninit(&impl_->state.device);
            impl_->state.device_initialized = false;
        }
    }
}

bool AudioPlaybackDevice::is_running() const noexcept {
    return impl_->state.running.load();
}

uint64_t AudioPlaybackDevice::frames_rendered() const noexcept {
    return impl_->state.frames_rendered.load();
}

uint64_t AudioPlaybackDevice::underruns() const noexcept {
    return impl_->state.underruns.load();
}

}  // namespace chorus

#include <chorus/codec/opus_codec.hpp>

#include <opus.h>

namespace chorus {

// -----------------------------------------------------------------------------
// OpusEncoderWrap
// -----------------------------------------------------------------------------

OpusEncoderWrap::OpusEncoderWrap() = default;

OpusEncoderWrap::~OpusEncoderWrap() {
    if (encoder_ != nullptr) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }
}

OpusEncoderWrap::OpusEncoderWrap(OpusEncoderWrap&& other) noexcept : encoder_(other.encoder_) {
    other.encoder_ = nullptr;
}

OpusEncoderWrap& OpusEncoderWrap::operator=(OpusEncoderWrap&& other) noexcept {
    if (this != &other) {
        if (encoder_ != nullptr) {
            opus_encoder_destroy(encoder_);
        }
        encoder_ = other.encoder_;
        other.encoder_ = nullptr;
    }
    return *this;
}

bool OpusEncoderWrap::init(int bitrate, int expected_loss_pct) {
    if (encoder_ != nullptr) {
        opus_encoder_destroy(encoder_);
        encoder_ = nullptr;
    }

    int error = OPUS_OK;
    encoder_ = opus_encoder_create(kSampleRate, kChannels, OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK || encoder_ == nullptr) {
        return false;
    }

    // Configure VBR, Bitrate, Complexity, and in-band FEC as specified in architecture.md section 5
    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(bitrate));                   // NOLINT(cppcoreguidelines-pro-type-vararg)
    opus_encoder_ctl(encoder_, OPUS_SET_VBR(1));                             // NOLINT(cppcoreguidelines-pro-type-vararg)
    opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(8));                      // NOLINT(cppcoreguidelines-pro-type-vararg)
    opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(1));                      // NOLINT(cppcoreguidelines-pro-type-vararg)
    opus_encoder_ctl(encoder_, OPUS_SET_PACKET_LOSS_PERC(expected_loss_pct)); // NOLINT(cppcoreguidelines-pro-type-vararg)

    return true;
}

int OpusEncoderWrap::encode(std::span<const float> pcm_in, std::span<uint8_t> out_payload) {
    if (encoder_ == nullptr || pcm_in.size() != static_cast<size_t>(kFloatsPerFrame)) {
        return -1;
    }

    const auto bytes = opus_encode_float(
        encoder_,
        pcm_in.data(),
        kSamplesPerFramePerChannel,
        out_payload.data(),
        static_cast<opus_int32>(out_payload.size())
    );

    return (bytes < 0) ? -1 : static_cast<int>(bytes);
}

// -----------------------------------------------------------------------------
// OpusDecoderWrap
// -----------------------------------------------------------------------------

OpusDecoderWrap::OpusDecoderWrap() = default;

OpusDecoderWrap::~OpusDecoderWrap() {
    if (decoder_ != nullptr) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }
}

OpusDecoderWrap::OpusDecoderWrap(OpusDecoderWrap&& other) noexcept : decoder_(other.decoder_) {
    other.decoder_ = nullptr;
}

OpusDecoderWrap& OpusDecoderWrap::operator=(OpusDecoderWrap&& other) noexcept {
    if (this != &other) {
        if (decoder_ != nullptr) {
            opus_decoder_destroy(decoder_);
        }
        decoder_ = other.decoder_;
        other.decoder_ = nullptr;
    }
    return *this;
}

bool OpusDecoderWrap::init() {
    if (decoder_ != nullptr) {
        opus_decoder_destroy(decoder_);
        decoder_ = nullptr;
    }

    int error = OPUS_OK;
    decoder_ = opus_decoder_create(kSampleRate, kChannels, &error);
    return (error == OPUS_OK && decoder_ != nullptr);
}

int OpusDecoderWrap::decode(std::span<const uint8_t> payload,
                           std::span<float> pcm_out,
                           bool decode_fec) {
    if (decoder_ == nullptr || pcm_out.size() < static_cast<size_t>(kFloatsPerFrame)) {
        return -1;
    }

    const auto* data = payload.empty() ? nullptr : payload.data();
    const auto len = static_cast<opus_int32>(payload.size());

    const int samples_decoded = opus_decode_float(
        decoder_,
        data,
        len,
        pcm_out.data(),
        kSamplesPerFramePerChannel,
        decode_fec ? 1 : 0
    );

    return samples_decoded;
}

}  // namespace chorus

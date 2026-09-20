#pragma once

#include <cstdint>
#include <span>

struct OpusEncoder;
struct OpusDecoder;

namespace chorus {

/// @brief Audio format constants for Chorus stream.
inline constexpr int kSampleRate = 48000;
inline constexpr int kChannels = 2;
inline constexpr int kFrameDurationMs = 20;
inline constexpr int kSamplesPerFramePerChannel = (kSampleRate * kFrameDurationMs) / 1000;  // 960
inline constexpr int kFloatsPerFrame = kSamplesPerFramePerChannel * kChannels;              // 1920
inline constexpr int kDefaultBitrate = 96000;                                               // 96 kbps
inline constexpr int kDefaultExpectedLossPct = 5;
inline constexpr int kMaxOpusPayloadBytes = 1200;

/// @brief Wrapper for Opus audio encoder (48kHz stereo, 20ms frames).
class OpusEncoderWrap {
public:
    OpusEncoderWrap();
    ~OpusEncoderWrap();

    OpusEncoderWrap(const OpusEncoderWrap&) = delete;
    OpusEncoderWrap& operator=(const OpusEncoderWrap&) = delete;
    OpusEncoderWrap(OpusEncoderWrap&& other) noexcept;
    OpusEncoderWrap& operator=(OpusEncoderWrap&& other) noexcept;

    /// @brief Initializes the encoder with specified bitrate and in-band FEC settings.
    [[nodiscard]] bool init(int bitrate = kDefaultBitrate,
                            int expected_loss_pct = kDefaultExpectedLossPct);

    /// @brief Encodes 960 stereo float samples (1920 floats) to an Opus payload.
    /// @param pcm_in Interleaved float samples (must be exactly 1920 floats).
    /// @param out_payload Buffer to receive encoded bytes.
    /// @return Number of encoded bytes, or -1 on error.
    [[nodiscard]] int encode(std::span<const float> pcm_in, std::span<uint8_t> out_payload);

private:
    OpusEncoder* encoder_{nullptr};
};

/// @brief Wrapper for Opus audio decoder (48kHz stereo, 20ms frames, PLC support).
class OpusDecoderWrap {
public:
    OpusDecoderWrap();
    ~OpusDecoderWrap();

    OpusDecoderWrap(const OpusDecoderWrap&) = delete;
    OpusDecoderWrap& operator=(const OpusDecoderWrap&) = delete;
    OpusDecoderWrap(OpusDecoderWrap&& other) noexcept;
    OpusDecoderWrap& operator=(OpusDecoderWrap&& other) noexcept;

    /// @brief Initializes the decoder.
    [[nodiscard]] bool init();

    /// @brief Decodes an Opus packet into interleaved float samples.
    /// If payload is empty, performs Packet Loss Concealment (PLC).
    /// @param payload Encoded Opus payload (or empty span for PLC).
    /// @param pcm_out Output buffer (must have space for at least 1920 floats).
    /// @param decode_fec If true and packet lost, attempt FEC decoding from next packet.
    /// @return Number of decoded samples per channel (960 on success), or -1 on error.
    [[nodiscard]] int decode(std::span<const uint8_t> payload,
                             std::span<float> pcm_out,
                             bool decode_fec = false);

    /// @brief Performs Packet Loss Concealment (PLC) synthesis for a lost frame.
    /// @param pcm_out Output buffer (must have space for at least 1920 floats).
    /// @return Number of decoded samples per channel (960 on success), or -1 on error.
    [[nodiscard]] int decode_plc(std::span<float> pcm_out) {
        return decode({}, pcm_out);
    }

private:
    OpusDecoder* decoder_{nullptr};
};

}  // namespace chorus

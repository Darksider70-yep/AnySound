#pragma once

#include <chorus/core.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace chorus {

inline constexpr size_t kCryptoKeySize = 32;       // 256-bit symmetric key
inline constexpr size_t kCryptoNonceSize = 12;     // 96-bit nonce
inline constexpr size_t kCryptoTagSize = 16;       // 128-bit Poly1305 MAC tag
inline constexpr size_t kCryptoOverhead = kCryptoNonceSize + kCryptoTagSize;

/// @brief Secure authenticated AEAD encryption and decryption channel (ChaCha20-Poly1305).
/// Provides tamper resistance, confidentiality, and anti-replay protection for UDP/TCP packets.
class CryptoChannel {
public:
    CryptoChannel();
    explicit CryptoChannel(std::span<const uint8_t, kCryptoKeySize> symmetric_key);
    ~CryptoChannel() = default;

    CryptoChannel(const CryptoChannel&) = default;
    CryptoChannel& operator=(const CryptoChannel&) = default;
    CryptoChannel(CryptoChannel&&) noexcept = default;
    CryptoChannel& operator=(CryptoChannel&&) noexcept = default;

    /// @brief Derives a 256-bit session key using HKDF-SHA256 from an ephemeral secret and optional PIN.
    static std::array<uint8_t, kCryptoKeySize> derive_session_key(
        std::span<const uint8_t> secret_material,
        std::string_view pin_salt,
        std::string_view info_context = "Chorus-v1-Transport-Key");

    /// @brief Sets the 256-bit symmetric encryption key.
    void set_key(std::span<const uint8_t, kCryptoKeySize> key);

    /// @brief Returns true if a valid symmetric key has been initialized.
    [[nodiscard]] bool is_initialized() const noexcept { return key_initialized_; }

    /// @brief Encrypts plaintext with authenticated Poly1305 tag and prepended 96-bit nonce.
    /// @param plaintext Buffer containing data to encrypt.
    /// @param aad Optional additional authenticated data (e.g. packet header).
    /// @return Ciphertext buffer [Nonce (12B) | Ciphertext | Tag (16B)].
    [[nodiscard]] std::vector<uint8_t> encrypt(
        std::span<const uint8_t> plaintext,
        std::span<const uint8_t> aad = {});

    /// @brief Decrypts and verifies authenticated ciphertext buffer.
    /// @param ciphertext_with_nonce_tag Encrypted buffer [Nonce (12B) | Ciphertext | Tag (16B)].
    /// @param aad Optional additional authenticated data.
    /// @return Decrypted plaintext, or std::nullopt if verification/tamper check fails or replay detected.
    [[nodiscard]] std::optional<std::vector<uint8_t>> decrypt(
        std::span<const uint8_t> ciphertext_with_nonce_tag,
        std::span<const uint8_t> aad = {});

    /// @brief In-place encryption writing into a destination buffer.
    /// @param plaintext Data to encrypt.
    /// @param aad Additional authenticated data.
    /// @param out_buffer Destination buffer, must have size >= plaintext.size() + kCryptoOverhead.
    /// @return Number of bytes written, or 0 on failure.
    size_t encrypt_to(
        std::span<const uint8_t> plaintext,
        std::span<const uint8_t> aad,
        std::span<uint8_t> out_buffer);

    /// @brief In-place decryption writing into a destination buffer.
    /// @param ciphertext_with_nonce_tag Encrypted buffer.
    /// @param aad Additional authenticated data.
    /// @param out_buffer Destination buffer, must have size >= ciphertext.size() - kCryptoOverhead.
    /// @return Number of decrypted plaintext bytes written, or 0 on verification/tamper failure.
    size_t decrypt_to(
        std::span<const uint8_t> ciphertext_with_nonce_tag,
        std::span<const uint8_t> aad,
        std::span<uint8_t> out_buffer);

    /// @brief Resets replay window and nonce counter.
    void reset();

private:
    std::array<uint8_t, kCryptoKeySize> key_{};
    bool key_initialized_{false};
    uint64_t tx_nonce_counter_{1};
    uint64_t max_rx_nonce_{0};
    uint64_t replay_window_mask_{0};  // 64-packet sliding window bitmask
};

}  // namespace chorus

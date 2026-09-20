#include <chorus/net/crypto_channel.hpp>

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace chorus;

TEST_CASE("CryptoChannel AEAD round-trip encryption and authentication", "[crypto]") {
    std::array<uint8_t, kCryptoKeySize> key = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };

    CryptoChannel tx_channel(key);
    CryptoChannel rx_channel(key);

    std::string plaintext_str = "Synchronized low-latency multi-device audio frame payload data";
    std::vector<uint8_t> plaintext(plaintext_str.begin(), plaintext_str.end());

    std::string aad_str = "PacketHeader:AUDIO:seq=12345";
    std::vector<uint8_t> aad(aad_str.begin(), aad_str.end());

    // 1. Encrypt
    auto encrypted = tx_channel.encrypt(plaintext, aad);
    REQUIRE(encrypted.size() == plaintext.size() + kCryptoOverhead);

    // 2. Decrypt & Authenticate
    auto decrypted_opt = rx_channel.decrypt(encrypted, aad);
    REQUIRE(decrypted_opt.has_value());
    REQUIRE(decrypted_opt.value() == plaintext);

    // 3. AAD Mismatch Tamper Check
    std::string bad_aad_str = "PacketHeader:AUDIO:seq=99999";
    std::vector<uint8_t> bad_aad(bad_aad_str.begin(), bad_aad_str.end());
    auto tampered_aad_opt = rx_channel.decrypt(encrypted, bad_aad);
    REQUIRE_FALSE(tampered_aad_opt.has_value());

    // 4. Ciphertext Bit Mutation Tamper Check
    auto tampered_encrypted = encrypted;
    tampered_encrypted[kCryptoNonceSize + 5] ^= 0xFF;  // Flip bits in ciphertext
    auto tampered_ct_opt = rx_channel.decrypt(tampered_encrypted, aad);
    REQUIRE_FALSE(tampered_ct_opt.has_value());

    // 5. Tag Mutation Tamper Check
    auto tampered_tag = encrypted;
    tampered_tag[tampered_tag.size() - 1] ^= 0x01;  // Flip bit in Poly1305 MAC tag
    auto tampered_tag_opt = rx_channel.decrypt(tampered_tag, aad);
    REQUIRE_FALSE(tampered_tag_opt.has_value());
}

TEST_CASE("CryptoChannel Replay Attack Prevention", "[crypto]") {
    std::array<uint8_t, kCryptoKeySize> key{};
    std::fill(key.begin(), key.end(), 0x42);

    CryptoChannel tx_channel(key);
    CryptoChannel rx_channel(key);

    std::vector<uint8_t> msg = {0xCA, 0xFE, 0xBA, 0xBE};
    auto packet = tx_channel.encrypt(msg);

    // First arrival: valid
    auto decrypted = rx_channel.decrypt(packet);
    REQUIRE(decrypted.has_value());

    // Duplicate/replay arrival: must be rejected
    auto replay = rx_channel.decrypt(packet);
    REQUIRE_FALSE(replay.has_value());
}

TEST_CASE("CryptoChannel Key Derivation with Salt", "[crypto]") {
    std::vector<uint8_t> secret = {0x10, 0x20, 0x30, 0x40, 0x50};
    auto key1 = CryptoChannel::derive_session_key(secret, "1234");
    auto key2 = CryptoChannel::derive_session_key(secret, "1234");
    auto key3 = CryptoChannel::derive_session_key(secret, "5678");

    REQUIRE(key1 == key2);
    REQUIRE(key1 != key3);
}

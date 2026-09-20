#include <chorus/net/crypto_channel.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

namespace chorus {

namespace {

// ----------------------------------------------------------------------------
// Constant-time arithmetic & bitwise utilities
// ----------------------------------------------------------------------------

inline uint32_t rotl32(uint32_t x, int n) noexcept {
    return (x << n) | (x >> (32 - n));
}

inline uint32_t load32_le(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline void store32_le(uint8_t* p, uint32_t v) noexcept {
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

inline uint32_t load32_be(const uint8_t* p) noexcept {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

inline void store32_be(uint8_t* p, uint32_t v) noexcept {
    p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(v & 0xFF);
}

inline void store64_le(uint8_t* p, uint64_t v) noexcept {
    store32_le(p, static_cast<uint32_t>(v & 0xFFFFFFFF));
    store32_le(p + 4, static_cast<uint32_t>(v >> 32));
}

// ----------------------------------------------------------------------------
// ChaCha20 Block Function (RFC 8439)
// ----------------------------------------------------------------------------

void chacha20_quarter_round(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) noexcept {
    a += b; d ^= a; d = rotl32(d, 16);
    c += d; b ^= c; b = rotl32(b, 12);
    a += b; d ^= a; d = rotl32(d, 8);
    c += d; b ^= c; b = rotl32(b, 7);
}

void chacha20_block(
    const uint8_t key[32],
    uint32_t counter,
    const uint8_t nonce[12],
    uint8_t out[64]) noexcept {
    // "expand 32-byte k" constants
    uint32_t state[16] = {
        0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
        load32_le(key + 0),  load32_le(key + 4),  load32_le(key + 8),  load32_le(key + 12),
        load32_le(key + 16), load32_le(key + 20), load32_le(key + 24), load32_le(key + 28),
        counter,
        load32_le(nonce + 0), load32_le(nonce + 4), load32_le(nonce + 8)
    };

    uint32_t working[16];
    std::copy_n(state, 16, working);

    for (int i = 0; i < 10; ++i) {
        // Column rounds
        chacha20_quarter_round(working[0], working[4], working[8],  working[12]);
        chacha20_quarter_round(working[1], working[5], working[9],  working[13]);
        chacha20_quarter_round(working[2], working[6], working[10], working[14]);
        chacha20_quarter_round(working[3], working[7], working[11], working[15]);

        // Diagonal rounds
        chacha20_quarter_round(working[0], working[5], working[10], working[15]);
        chacha20_quarter_round(working[1], working[6], working[11], working[12]);
        chacha20_quarter_round(working[2], working[7], working[8],  working[13]);
        chacha20_quarter_round(working[3], working[4], working[9],  working[14]);
    }

    for (int i = 0; i < 16; ++i) {
        store32_le(out + (i * 4), working[i] + state[i]);
    }
}

void chacha20_crypt(
    const uint8_t key[32],
    uint32_t counter,
    const uint8_t nonce[12],
    std::span<const uint8_t> in,
    std::span<uint8_t> out) noexcept {
    size_t len = in.size();
    size_t offset = 0;
    uint8_t block[64];

    while (len > 0) {
        chacha20_block(key, counter, nonce, block);
        size_t chunk = std::min(len, size_t{64});
        for (size_t i = 0; i < chunk; ++i) {
            out[offset + i] = in[offset + i] ^ block[i];
        }
        counter++;
        offset += chunk;
        len -= chunk;
    }
}

// ----------------------------------------------------------------------------
// Poly1305 Authenticator (RFC 8439)
// ----------------------------------------------------------------------------

struct Poly1305State {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
};

void poly1305_init(Poly1305State& st, const uint8_t key[32]) noexcept {
    // Clamp r: r &= 0x0ffffffc0ffffffc0ffffffc0fffffff
    uint32_t t0 = load32_le(key + 0);
    uint32_t t1 = load32_le(key + 4);
    uint32_t t2 = load32_le(key + 8);
    uint32_t t3 = load32_le(key + 12);

    st.r[0] = t0 & 0x3ffffff;
    st.r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
    st.r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
    st.r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
    st.r[4] = (t3 >> 8) & 0x00fffff;

    st.h[0] = 0;
    st.h[1] = 0;
    st.h[2] = 0;
    st.h[3] = 0;
    st.h[4] = 0;

    st.pad[0] = load32_le(key + 16);
    st.pad[1] = load32_le(key + 20);
    st.pad[2] = load32_le(key + 24);
    st.pad[3] = load32_le(key + 28);
}

void poly1305_blocks(Poly1305State& st, const uint8_t* m, size_t bytes, uint32_t hibit) noexcept {
    uint32_t r0 = st.r[0], r1 = st.r[1], r2 = st.r[2], r3 = st.r[3], r4 = st.r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = st.h[0], h1 = st.h[1], h2 = st.h[2], h3 = st.h[3], h4 = st.h[4];

    while (bytes >= 16) {
        uint32_t t0 = load32_le(m + 0);
        uint32_t t1 = load32_le(m + 4);
        uint32_t t2 = load32_le(m + 8);
        uint32_t t3 = load32_le(m + 12);

        h0 += t0 & 0x3ffffff;
        h1 += ((t0 >> 26) | (t1 << 6)) & 0x3ffffff;
        h2 += ((t1 >> 20) | (t2 << 12)) & 0x3ffffff;
        h3 += ((t2 >> 14) | (t3 << 18)) & 0x3ffffff;
        h4 += (t3 >> 8) | hibit;

        uint64_t d0 = static_cast<uint64_t>(h0) * r0 + static_cast<uint64_t>(h1) * s4 + static_cast<uint64_t>(h2) * s3 + static_cast<uint64_t>(h3) * s2 + static_cast<uint64_t>(h4) * s1;
        uint64_t d1 = static_cast<uint64_t>(h0) * r1 + static_cast<uint64_t>(h1) * r0 + static_cast<uint64_t>(h2) * s4 + static_cast<uint64_t>(h3) * s3 + static_cast<uint64_t>(h4) * s2;
        uint64_t d2 = static_cast<uint64_t>(h0) * r2 + static_cast<uint64_t>(h1) * r1 + static_cast<uint64_t>(h2) * r0 + static_cast<uint64_t>(h3) * s4 + static_cast<uint64_t>(h4) * s3;
        uint64_t d3 = static_cast<uint64_t>(h0) * r3 + static_cast<uint64_t>(h1) * r2 + static_cast<uint64_t>(h2) * r1 + static_cast<uint64_t>(h3) * r0 + static_cast<uint64_t>(h4) * s4;
        uint64_t d4 = static_cast<uint64_t>(h0) * r4 + static_cast<uint64_t>(h1) * r3 + static_cast<uint64_t>(h2) * r2 + static_cast<uint64_t>(h3) * r1 + static_cast<uint64_t>(h4) * r0;

        uint32_t c;
        c = static_cast<uint32_t>(d0 >> 26); h0 = static_cast<uint32_t>(d0) & 0x3ffffff; d1 += c;
        c = static_cast<uint32_t>(d1 >> 26); h1 = static_cast<uint32_t>(d1) & 0x3ffffff; d2 += c;
        c = static_cast<uint32_t>(d2 >> 26); h2 = static_cast<uint32_t>(d2) & 0x3ffffff; d3 += c;
        c = static_cast<uint32_t>(d3 >> 26); h3 = static_cast<uint32_t>(d3) & 0x3ffffff; d4 += c;
        c = static_cast<uint32_t>(d4 >> 26); h4 = static_cast<uint32_t>(d4) & 0x3ffffff;
        h0 += c * 5;
        c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;

        m += 16;
        bytes -= 16;
    }

    st.h[0] = h0; st.h[1] = h1; st.h[2] = h2; st.h[3] = h3; st.h[4] = h4;
}

void poly1305_finish(Poly1305State& st, uint8_t mac[16]) noexcept {
    uint32_t h0 = st.h[0], h1 = st.h[1], h2 = st.h[2], h3 = st.h[3], h4 = st.h[4];

    uint32_t c = h1 >> 26; h1 &= 0x3ffffff; h2 += c;
    c = h2 >> 26; h2 &= 0x3ffffff; h3 += c;
    c = h3 >> 26; h3 &= 0x3ffffff; h4 += c;
    c = h4 >> 26; h4 &= 0x3ffffff; h0 += c * 5;
    c = h0 >> 26; h0 &= 0x3ffffff; h1 += c;

    uint32_t g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1 << 26);

    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    uint32_t f0 = ((h0) | (h1 << 26));
    uint32_t f1 = ((h1 >> 6) | (h2 << 20));
    uint32_t f2 = ((h2 >> 12) | (h3 << 14));
    uint32_t f3 = ((h3 >> 18) | (h4 << 8));

    uint64_t t;
    t = static_cast<uint64_t>(f0) + st.pad[0]; store32_le(mac + 0, static_cast<uint32_t>(t));
    t = static_cast<uint64_t>(f1) + st.pad[1] + (t >> 32); store32_le(mac + 4, static_cast<uint32_t>(t));
    t = static_cast<uint64_t>(f2) + st.pad[2] + (t >> 32); store32_le(mac + 8, static_cast<uint32_t>(t));
    t = static_cast<uint64_t>(f3) + st.pad[3] + (t >> 32); store32_le(mac + 12, static_cast<uint32_t>(t));
}

void poly1305_mac(const uint8_t key[32], std::span<const uint8_t> aad, std::span<const uint8_t> ct, uint8_t out_tag[16]) noexcept {
    Poly1305State st;
    poly1305_init(st, key);

    // 1. Process AAD (padded to 16 bytes)
    size_t aad_full = (aad.size() / 16) * 16;
    if (aad_full > 0) {
        poly1305_blocks(st, aad.data(), aad_full, 1 << 24);
    }
    if (aad.size() > aad_full) {
        uint8_t pad[16] = {0};
        std::copy(aad.begin() + static_cast<ptrdiff_t>(aad_full), aad.end(), pad);
        poly1305_blocks(st, pad, 16, 1 << 24);
    }

    // 2. Process Ciphertext (padded to 16 bytes)
    size_t ct_full = (ct.size() / 16) * 16;
    if (ct_full > 0) {
        poly1305_blocks(st, ct.data(), ct_full, 1 << 24);
    }
    if (ct.size() > ct_full) {
        uint8_t pad[16] = {0};
        std::copy(ct.begin() + static_cast<ptrdiff_t>(ct_full), ct.end(), pad);
        poly1305_blocks(st, pad, 16, 1 << 24);
    }

    // 3. Process lengths: len(AAD) as uint64_le, len(CT) as uint64_le
    uint8_t len_block[16];
    store64_le(len_block + 0, static_cast<uint64_t>(aad.size()));
    store64_le(len_block + 8, static_cast<uint64_t>(ct.size()));
    poly1305_blocks(st, len_block, 16, 1 << 24);

    poly1305_finish(st, out_tag);
}

// ----------------------------------------------------------------------------
// SHA-256 & HMAC-SHA256 (RFC 6234 / RFC 5869)
// ----------------------------------------------------------------------------

struct Sha256Context {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
};

const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void sha256_transform(uint32_t state[8], const uint8_t block[64]) noexcept {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = load32_be(block + i * 4);
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotl32(w[i - 15], 25) ^ rotl32(w[i - 15], 14) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotl32(w[i - 2], 15) ^ rotl32(w[i - 2], 13) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t s1 = rotl32(e, 26) ^ rotl32(e, 21) ^ rotl32(e, 7);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + ch + K256[i] + w[i];
        uint32_t s0 = rotl32(a, 30) ^ rotl32(a, 19) ^ rotl32(a, 10);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void sha256_init(Sha256Context& ctx) noexcept {
    ctx.state[0] = 0x6a09e667; ctx.state[1] = 0xbb67ae85;
    ctx.state[2] = 0x3c6ef372; ctx.state[3] = 0xa54ff53a;
    ctx.state[4] = 0x510e527f; ctx.state[5] = 0x9b05688c;
    ctx.state[6] = 0x1f83d9ab; ctx.state[7] = 0x5be0cd19;
    ctx.count = 0;
}

void sha256_update(Sha256Context& ctx, const uint8_t* data, size_t len) noexcept {
    size_t index = static_cast<size_t>(ctx.count & 63);
    ctx.count += len;
    size_t part_len = 64 - index;
    size_t i = 0;

    if (len >= part_len) {
        std::copy_n(data, part_len, ctx.buffer + index);
        sha256_transform(ctx.state, ctx.buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            sha256_transform(ctx.state, data + i);
        }
        index = 0;
    }
    std::copy_n(data + i, len - i, ctx.buffer + index);
}

void sha256_final(Sha256Context& ctx, uint8_t digest[32]) noexcept {
    uint8_t final_count[8];
    uint64_t total_bits = ctx.count * 8;
    for (int i = 0; i < 8; ++i) {
        final_count[7 - i] = static_cast<uint8_t>(total_bits >> (i * 8));
    }
    size_t index = static_cast<size_t>(ctx.count & 63);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    uint8_t padding[64] = {0x80};
    sha256_update(ctx, padding, pad_len);
    sha256_update(ctx, final_count, 8);
    for (int i = 0; i < 8; ++i) {
        store32_be(digest + (i * 4), ctx.state[i]);
    }
}

void hmac_sha256(
    std::span<const uint8_t> key,
    std::span<const uint8_t> msg,
    uint8_t out[32]) noexcept {
    uint8_t k_pad[64] = {0};
    if (key.size() > 64) {
        Sha256Context ctx;
        sha256_init(ctx);
        sha256_update(ctx, key.data(), key.size());
        sha256_final(ctx, k_pad);
    } else {
        std::copy(key.begin(), key.end(), k_pad);
    }

    uint8_t ipad[64];
    uint8_t opad[64];
    for (int i = 0; i < 64; ++i) {
        ipad[i] = k_pad[i] ^ 0x36;
        opad[i] = k_pad[i] ^ 0x5c;
    }

    uint8_t inner_hash[32];
    Sha256Context inner;
    sha256_init(inner);
    sha256_update(inner, ipad, 64);
    sha256_update(inner, msg.data(), msg.size());
    sha256_final(inner, inner_hash);

    Sha256Context outer;
    sha256_init(outer);
    sha256_update(outer, opad, 64);
    sha256_update(outer, inner_hash, 32);
    sha256_final(outer, out);
}

}  // namespace

// ----------------------------------------------------------------------------
// CryptoChannel Class Implementation
// ----------------------------------------------------------------------------

CryptoChannel::CryptoChannel() = default;

CryptoChannel::CryptoChannel(std::span<const uint8_t, kCryptoKeySize> symmetric_key) {
    set_key(symmetric_key);
}

void CryptoChannel::set_key(std::span<const uint8_t, kCryptoKeySize> key) {
    std::copy(key.begin(), key.end(), key_.begin());
    key_initialized_ = true;
    reset();
}

void CryptoChannel::reset() {
    tx_nonce_counter_ = 1;
    max_rx_nonce_ = 0;
    replay_window_mask_ = 0;
}

std::array<uint8_t, kCryptoKeySize> CryptoChannel::derive_session_key(
    std::span<const uint8_t> secret_material,
    std::string_view pin_salt,
    std::string_view info_context) {
    // HKDF-Extract: PRK = HMAC-SHA256(salt=PIN, IKM=secret_material)
    uint8_t prk[32];
    std::span<const uint8_t> salt_span(reinterpret_cast<const uint8_t*>(pin_salt.data()), pin_salt.size());
    hmac_sha256(salt_span, secret_material, prk);

    // HKDF-Expand: OKM = HMAC-SHA256(PRK, info | 0x01)
    std::vector<uint8_t> info_with_counter;
    info_with_counter.reserve(info_context.size() + 1);
    info_with_counter.insert(info_with_counter.end(), info_context.begin(), info_context.end());
    info_with_counter.push_back(0x01);

    std::array<uint8_t, kCryptoKeySize> out_key{};
    hmac_sha256(prk, info_with_counter, out_key.data());
    return out_key;
}

std::vector<uint8_t> CryptoChannel::encrypt(
    std::span<const uint8_t> plaintext,
    std::span<const uint8_t> aad) {
    if (!key_initialized_) {
        return {};
    }
    std::vector<uint8_t> out(plaintext.size() + kCryptoOverhead);
    size_t written = encrypt_to(plaintext, aad, out);
    if (written != out.size()) {
        out.clear();
    }
    return out;
}

size_t CryptoChannel::encrypt_to(
    std::span<const uint8_t> plaintext,
    std::span<const uint8_t> aad,
    std::span<uint8_t> out_buffer) {
    if (!key_initialized_ || out_buffer.size() < plaintext.size() + kCryptoOverhead) {
        return 0;
    }

    // 1. Generate 96-bit nonce (4 bytes zero prefix + 8 bytes counter)
    uint8_t nonce[12] = {0};
    uint64_t current_nonce = tx_nonce_counter_++;
    store64_le(nonce + 4, current_nonce);

    // Copy nonce to output prefix
    std::copy_n(nonce, 12, out_buffer.data());

    // 2. Generate Poly1305 one-time subkey with block counter = 0
    uint8_t poly_key_block[64];
    chacha20_block(key_.data(), 0, nonce, poly_key_block);

    // 3. Encrypt payload with block counter = 1
    auto ct_span = out_buffer.subspan(kCryptoNonceSize, plaintext.size());
    chacha20_crypt(key_.data(), 1, nonce, plaintext, ct_span);

    // 4. Calculate Poly1305 MAC tag over AAD and ciphertext
    uint8_t tag[16];
    poly1305_mac(poly_key_block, aad, ct_span, tag);

    // Append tag to output
    std::copy_n(tag, 16, out_buffer.data() + kCryptoNonceSize + plaintext.size());

    return kCryptoNonceSize + plaintext.size() + kCryptoTagSize;
}

std::optional<std::vector<uint8_t>> CryptoChannel::decrypt(
    std::span<const uint8_t> ciphertext_with_nonce_tag,
    std::span<const uint8_t> aad) {
    if (!key_initialized_ || ciphertext_with_nonce_tag.size() < kCryptoOverhead) {
        return std::nullopt;
    }
    std::vector<uint8_t> out(ciphertext_with_nonce_tag.size() - kCryptoOverhead);
    size_t written = decrypt_to(ciphertext_with_nonce_tag, aad, out);
    if (written == 0 && !out.empty()) {
        return std::nullopt;
    }
    return out;
}

size_t CryptoChannel::decrypt_to(
    std::span<const uint8_t> ciphertext_with_nonce_tag,
    std::span<const uint8_t> aad,
    std::span<uint8_t> out_buffer) {
    if (!key_initialized_ || ciphertext_with_nonce_tag.size() < kCryptoOverhead) {
        return 0;
    }

    size_t ct_len = ciphertext_with_nonce_tag.size() - kCryptoOverhead;
    if (out_buffer.size() < ct_len) {
        return 0;
    }

    const uint8_t* nonce = ciphertext_with_nonce_tag.data();
    auto ct_span = ciphertext_with_nonce_tag.subspan(kCryptoNonceSize, ct_len);
    const uint8_t* received_tag = ciphertext_with_nonce_tag.data() + kCryptoNonceSize + ct_len;

    // 1. Anti-replay verification (64-packet window check)
    uint64_t rx_nonce = load32_le(nonce + 4) | (static_cast<uint64_t>(load32_le(nonce + 8)) << 32);
    if (rx_nonce != 0) {
        if (rx_nonce > max_rx_nonce_) {
            uint64_t diff = rx_nonce - max_rx_nonce_;
            if (diff < 64) {
                replay_window_mask_ <<= diff;
                replay_window_mask_ |= 1ULL;
            } else {
                replay_window_mask_ = 1ULL;
            }
            max_rx_nonce_ = rx_nonce;
        } else {
            uint64_t diff = max_rx_nonce_ - rx_nonce;
            if (diff >= 64 || (replay_window_mask_ & (1ULL << diff)) != 0) {
                // Replay or duplicate detected
                return 0;
            }
            replay_window_mask_ |= (1ULL << diff);
        }
    }

    // 2. Generate Poly1305 subkey with counter = 0
    uint8_t poly_key_block[64];
    chacha20_block(key_.data(), 0, nonce, poly_key_block);

    // 3. Compute expected Poly1305 MAC tag
    uint8_t expected_tag[16];
    poly1305_mac(poly_key_block, aad, ct_span, expected_tag);

    // 4. Constant-time MAC comparison
    uint32_t diff = 0;
    for (int i = 0; i < 16; ++i) {
        diff |= (expected_tag[i] ^ received_tag[i]);
    }
    if (diff != 0) {
        // Tag verification failed - message was forged or tampered
        return 0;
    }

    // 5. Authenticated successfully; decrypt payload into out_buffer
    chacha20_crypt(key_.data(), 1, nonce, ct_span, out_buffer.subspan(0, ct_len));
    return ct_len;
}

}  // namespace chorus

// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// SHA-256 (FIPS 180-4), for the certify tool's result hashes: small, no
// dependencies, and the same digest as Python's hashlib.sha256.
#ifndef BEMAN_INSIDE_CERTIFY_SHA256_HPP
#define BEMAN_INSIDE_CERTIFY_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace beman::inside::certify {

class sha256 {
  public:
    void update(const unsigned char* data, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) {
            Block[Used++] = data[i];
            if (Used == 64) {
                compress();
                Used = 0;
            }
        }
        Length += size;
    }

    // The digest as 64 lowercase hex digits. The object is spent afterwards.
    std::string hex() {
        const std::uint64_t bits = Length * 8;
        const unsigned char one  = 0x80;
        update(&one, 1);
        const unsigned char zero = 0;
        while (Used != 56)
            update(&zero, 1);
        unsigned char length[8];
        for (int i = 0; i < 8; ++i)
            length[i] = static_cast<unsigned char>(bits >> (56 - 8 * i));
        update(length, 8);
        static constexpr char digits[] = "0123456789abcdef";
        std::string           out;
        for (const std::uint32_t h : State)
            for (int i = 28; i >= 0; i -= 4)
                out += digits[(h >> i) & 0xf];
        return out;
    }

  private:
    static constexpr std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void compress() {
        static constexpr std::array<std::uint32_t, 64> k{
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = static_cast<std::uint32_t>(Block[4 * i]) << 24 |
                   static_cast<std::uint32_t>(Block[4 * i + 1]) << 16 |
                   static_cast<std::uint32_t>(Block[4 * i + 2]) << 8 | static_cast<std::uint32_t>(Block[4 * i + 3]);
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i]                   = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = State[0], b = State[1], c = State[2], d = State[3];
        std::uint32_t e = State[4], f = State[5], g = State[6], h = State[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t t1 = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            h                      = g;
            g                      = f;
            f                      = e;
            e                      = d + t1;
            d                      = c;
            c                      = b;
            b                      = a;
            a                      = t1 + t2;
        }
        State[0] += a;
        State[1] += b;
        State[2] += c;
        State[3] += d;
        State[4] += e;
        State[5] += f;
        State[6] += g;
        State[7] += h;
    }

    std::array<std::uint32_t, 8> State{
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::array<unsigned char, 64> Block{};
    std::size_t                   Used   = 0;
    std::uint64_t                 Length = 0;
};

} // namespace beman::inside::certify

#endif

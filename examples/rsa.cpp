// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Textbook RSA-1024 on exact integer grids. The types carry the sizes:
// a residue mod n is inside<{0, n - 1}>, the product of two residues is a
// 2048-bit grid without anyone writing it, and "% n" lands back in
// [0, n - 1] as a plain value, since the point divisor n cannot be 0.
//   1. The private exponent d = e^-1 mod phi(n) by extended Euclid, with
//      divmod on 1024-bit grids.
//   2. Encrypt and decrypt a message: m^e and c^d mod n by square-and-multiply.
//      The encryption also runs at compile time: the arithmetic is constexpr.
//   3. Decrypt again by the Chinese remainder theorem on 512-bit residues: the
//      grid of m_q + q * h is exactly the grid of residues mod n.
//   4. Sign and verify.
// This is not cryptography: a fixed test key, no padding, not constant-time.

#include <iostream>
#include <string>
#include <string_view>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

// The test key: two 512-bit primes, made with Python's random module from
// seed 1024 (not a secret, not for use).
constexpr auto P =
    11593995768512807176495193119162431506583664557643191747914725129316042719710315266793819966352295590610036539054051400435059844065957971582735710464378453_g;
constexpr auto Q =
    13369845000199694860564159535075528293965934217442269357435729502091565764279300802400687522372144738442778276112919018543959746675970806535562254731138417_g;
constexpr auto N   = P * Q;
constexpr auto Phi = (P - 1) * (Q - 1);
constexpr auto E   = 65537;

constexpr auto Two1024 = [] {
    auto v = 2_g;
    for (int i = 0; i < 10; ++i)
        v = v * v; // 2^(2^i)
    return v;
}();
static_assert(Two1024 / 2 <= N && N < Two1024); // a 1024-bit modulus

using residue  = inside<{0, N - 1}>;   // Z/nZ
using modulus  = inside<{N, N}>;       // the point n: "% modulus{}" never divides by 0
using exponent = inside<{0, Phi - 1}>; // exponents mod phi(n)
using two      = inside<{2, 2}>;

// b^e mod M by right-to-left square-and-multiply. One template serves the
// residues mod n and the 512-bit residues mod p and q.
template <typename Residue, typename Modulus, typename Exponent>
constexpr Residue powmod(Residue b, Exponent e) {
    Residue r = 1;
    while (e != 0) {
        if (e % two{} == 1)
            r = r * b % Modulus{}; // a 2048-bit product, back onto [0, M - 1]
        b = b * b % Modulus{};
        e = div(e, two{}, round_toward_zero);
    }
    return r;
}

// d = e^-1 mod phi by extended Euclid. The remainders stay in [0, phi], the
// Bezout coefficients in [-phi, phi]: each step checks its way back there.
exponent inverse_mod_phi() {
    using rem  = inside<{0, Phi}>;
    using coef = inside<{-Phi, Phi}>;
    rem  r0 = Phi, r1 = E; // the grid number phi, read at run time
    coef t0 = 0, t1 = 1;
    while (r1 != 0) {
        const auto [q, r] = *divmod(r0, r1); // one 1024-bit division
        r0                = r1;
        r1                = r;
        const coef t      = checked_cast<coef>(t0 - q * t1);
        t0                = t1;
        t1                = t;
    }
    if (r0 != 1)
        throw std::logic_error("e is not invertible mod phi");
    return checked_cast<exponent>(mod(t0, inside<{Phi, Phi}>{}, round_floor));
}

// Bytes in, big-endian: m = m * 256 + byte, checked to stay below n.
constexpr residue pack(std::string_view text) {
    residue m = 0;
    for (const unsigned char c : text)
        m = checked_cast<residue>(m * inside<{256, 256}>{} + inside<{0, 255}>{c});
    return m;
}

std::string unpack(residue m) {
    std::string text;
    while (m != 0) {
        const auto [rest, byte] = divmod(m, inside<{256, 256}>{});
        text.insert(text.begin(), static_cast<char>(byte.as<unsigned char>()));
        m = rest;
    }
    return text;
}

// The first and last 12 of a number's digits.
std::string digits(const residue& x) {
    const std::string s = to_string(x);
    return s.substr(0, 12) + "..." + s.substr(s.size() - 12) + " (" + std::to_string(s.size()) + " digits)";
}

int main() {
    // 1. The private exponent.
    const exponent d = inverse_mod_phi();
    if (exponent{E} * d % inside<{Phi, Phi}>{} != 1)
        return 1;

    // 2. Encrypt and decrypt.
    constexpr std::string_view text = "inside: RSA-1024 on C++26 big grids";
    const residue              m    = pack(text);
    const residue              c    = powmod<residue, modulus>(m, exponent{E});
    // The same encryption during compilation: 17 squarings of 1024-bit residues.
    constexpr residue c_at_compile_time = powmod<residue, modulus>(pack(text), exponent{E});
    if (c != c_at_compile_time)
        return 1;
    const residue back = powmod<residue, modulus>(c, d);
    std::cout << "ciphertext = " << digits(c) << "\n";
    std::cout << "decrypted  = \"" << unpack(back) << "\"\n";
    if (digits(c) != "538545052492...964655171101 (308 digits)" || unpack(back) != text)
        return 1;

    // 3. CRT: two 512-bit exponentiations instead of one 1024-bit one.
    using residue_p     = inside<{0, P - 1}>;
    using residue_q     = inside<{0, Q - 1}>;
    using modulus_p     = inside<{P, P}>;
    using modulus_q     = inside<{Q, Q}>;
    using exponent_p    = inside<{0, P - 2}>;
    using exponent_q    = inside<{0, Q - 2}>;
    const exponent_p dp = d % inside<{P - 1, P - 1}>{};
    const exponent_q dq = d % inside<{Q - 1, Q - 1}>{};
    // q^-1 mod p by Fermat: q^(p - 2). (A big value forms at compile time
    // only, so p - 2 is a constant; an inside reads it at run time.)
    constexpr auto  fermat = P - 2;
    const residue_p qinv   = powmod<residue_p, modulus_p>(modulus_q{} % modulus_p{}, exponent_p{fermat});
    const residue_p mp     = powmod<residue_p, modulus_p>(c % modulus_p{}, dp);
    const residue_q mq     = powmod<residue_q, modulus_q>(c % modulus_q{}, dq);
    const residue_p h      = checked_cast<residue_p>(mod((mp - mq) * qinv, modulus_p{}, round_floor));
    const auto      crt    = mq + modulus_q{} * h; // on [0, q - 1 + q * (p - 1)] = [0, n - 1]
    static_assert(grid_of<decltype(crt)> == grid_of<residue>);
    std::cout << "CRT agrees = " << std::boolalpha << (crt == back) << "\n";
    if (crt != back)
        return 1;

    // 4. Sign with d, verify with e.
    const residue hash = pack("signed by beman.inside");
    const residue sig  = powmod<residue, modulus>(hash, d);
    const bool    ok   = powmod<residue, modulus>(sig, exponent{E}) == hash;
    std::cout << "signature  = " << digits(sig) << "\nverified   = " << ok << "\n";
    return ok && digits(sig) == "147061418896...979242965584 (309 digits)" ? 0 : 1;
}

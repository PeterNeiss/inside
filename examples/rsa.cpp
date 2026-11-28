// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Textbook RSA-1024 on exact integer grids: a residue mod n is id<key{grid{0, n - 1}}>,
// the product of two residues is a 2048-bit grid without anyone writing it,
// and "% n" lands back in [0, n - 1] as a plain value, since the point divisor n cannot be 0.
// Not cryptography: a fixed test key, no padding, not constant-time.

#include <expected>
#include <iostream>
#include <string>
#include <string_view>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>

using namespace ins;

// The test key: two 512-bit primes, made with Python's random module from seed 1024 (not a secret, not for use).
constexpr auto P =
  11593995768512807176495193119162431506583664557643191747914725129316042719710315266793819966352295590610036539054051400435059844065957971582735710464378453_g;
constexpr auto Q =
  13369845000199694860564159535075528293965934217442269357435729502091565764279300802400687522372144738442778276112919018543959746675970806535562254731138417_g;
constexpr auto N   = P * Q;
constexpr auto Phi = (P - 1) * (Q - 1);
constexpr auto E   = 65537;

constexpr auto Two1024 = 0x1p1024_g;
static_assert(Two1024 / 2 <= N && N < Two1024); // a 1024-bit modulus

using residue  = id<key{grid{0, N - 1  }}>; // Z/nZ
using modulus  = id<key{grid{N, N      }}>; // the point n: "% modulus{}" never divides by 0
using totient  = id<key{grid{Phi, Phi  }}>; // the point phi(n)
using exponent = id<key{grid{0, Phi - 1}}>; // exponents mod phi(n)

// b^e mod M by right-to-left square-and-multiply, for the residues mod n and the 512-bit ones mod p and q.
template <typename Residue, typename Modulus, typename Exponent>
constexpr Residue powmod(Residue b, Exponent e)
{
  Residue r = 1;
  while (e != 0)
  {
    if (e % 2_q == 1)
      r = r * b % Modulus{}; // a 2048-bit product, back onto [0, M - 1]
    b = b * b % Modulus{};
    e = div(e, 2_q, round_toward_zero);
  }
  return r;
}

// d = e^-1 mod phi by extended Euclid:
// the remainders stay in [0, phi], the Bezout coefficients in [-phi, phi],
// and each step checks its way back there (exact_cast, an expected).
// An e without an inverse is a domain_error.
std::expected<exponent, error> inverse_mod_phi()
{
  using remainder          = id<key{grid{0   , Phi}}>;
  using bezout_coefficient = id<key{grid{-Phi, Phi}}>;
  remainder          r0 = Phi, r1 = E; // the grid number phi, read at run time
  bezout_coefficient t0 = 0  , t1 = 1;
  while (r1 != 0)
  {
    const auto [q, r] = *divmod(r0, r1); // one 1024-bit division
    r0 = r1;
    r1 = r;
    const auto t{exact_cast<bezout_coefficient>(t0 - q * t1)};
    if (!t)
      return std::unexpected(t.error());
    t0 = t1;
    t1 = *t;
  }
  if (r0 != 1)
    return std::unexpected(error::domain_error);
  return exact_cast<exponent>(mod(t0, totient{}, round_floor));
}

// Bytes in, big-endian: m = m * 256 + byte, checked to stay below n.
constexpr std::expected<residue, error> pack(std::string_view text)
{
  residue m = 0;
  for (const unsigned char c : text)
  {
    const auto next{exact_cast<residue>(m * 256_q + id<"[0..255]">{c})};
    if (!next)
      return next;
    m = *next;
  }
  return m;
}

std::string unpack(residue m)
{
  std::string text;
  while (m != 0)
  {
    const auto [rest, byte] = divmod(m, 256_q);
    text.insert(text.begin(), static_cast<char>(byte.to<unsigned char>()));
    m = rest;
  }
  return text;
}

// The first and last 12 of a number's digits.
std::string digits(const residue& x)
{
  const std::string s{to_string(x)};
  return s.substr(0, 12) + "..." + s.substr(s.size() - 12) + " (" + std::to_string(s.size()) + " digits)";
}

int main()
{
  // 1. The private exponent d, by extended Euclid with divmod on 1024-bit grids.
  const auto checked_d{inverse_mod_phi()};
  if (!checked_d)
    return 1;
  const exponent d{*checked_d};
  if (exponent{E} * d % std::cw<Phi> != 1)
    return 1;

  // 2. Encrypt and decrypt, m^e and c^d mod n; the encryption also runs at compile time (17 squarings).
  constexpr std::string_view text = "inside: RSA-1024 on C++26 big grids";
  const auto packed{pack(text)};
  if (!packed)
    return 1;
  const residue     m{*packed};
  const residue     c{powmod<residue, modulus>(m, exponent{E})};
  constexpr residue c_at_compile_time{powmod<residue, modulus>(*pack(text), exponent{E})};
  if (c != c_at_compile_time)
    return 1;
  const residue back{powmod<residue, modulus>(c, d)};
  std::cout << "ciphertext = " << digits(c) << "\n";
  std::cout << "decrypted  = \"" << unpack(back) << "\"\n";
  if (digits(c) != "538545052492...964655171101 (308 digits)" || unpack(back) != text)
    return 1;

  // 3. Decrypt again by the Chinese remainder theorem: two 512-bit exponentiations instead of one 1024-bit one.
  using residue_p  = id<key{grid{0, P - 1}}>;
  using residue_q  = id<key{grid{0, Q - 1}}>;
  using modulus_p  = id<key{grid{P, P    }}>;
  using modulus_q  = id<key{grid{Q, Q    }}>;
  using exponent_p = id<key{grid{0, P - 2}}>;
  using exponent_q = id<key{grid{0, Q - 2}}>;
  const exponent_p dp = d % std::cw<P - 1>;
  const exponent_q dq = d % std::cw<Q - 1>;
  // q^-1 mod p by Fermat: q^(p - 2), with P - 2 formed during compilation.
  const residue_p qinv{powmod<residue_p, modulus_p>(modulus_q{} % modulus_p{}, exponent_p{P - 2})};
  const residue_p mp  {powmod<residue_p, modulus_p>(c % modulus_p{}, dp)};
  const residue_q mq  {powmod<residue_q, modulus_q>(c % modulus_q{}, dq)};
  const residue_p h   {exact_cast<residue_p>(mod((mp - mq) * qinv, modulus_p{}, round_floor))};
  const auto      crt {mq + modulus_q{} * h}; // on [0, q - 1 + q * (p - 1)] = [0, n - 1]
  static_assert(grid_of<decltype(crt)> == grid_of<residue>);
  std::cout << "CRT agrees = " << std::boolalpha << (crt == back) << "\n";
  if (crt != back)
    return 1;

  // 4. Sign with d, verify with e.
  const auto packed_hash{pack("signed by inside")};
  if (!packed_hash)
    return 1;
  const residue hash{*packed_hash};
  const residue sig {powmod<residue, modulus>(hash, d)};
  const bool    ok  {powmod<residue, modulus>(sig, exponent{E}) == hash};
  std::cout << "signature  = " << digits(sig) << "\nverified   = " << ok << "\n";
  return ok && digits(sig) == "754586494100...154302961483 (308 digits)" ? 0 : 1;
}

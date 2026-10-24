// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_WIDE_INT_HPP
#define BEMAN_INSIDE_DETAIL_WIDE_INT_HPP

#include <beman/inside/detail/config.hpp>
BEMAN_INSIDE_WARNINGS_PUSH

#include <beman/inside/numbers.hpp>                // umax
#include <beman/inside/detail/float_bits.hpp>      // constexpr ldexp
#include <beman/inside/detail/debug.hpp>           // errc, raise, constexpr_error
#include <beman/inside/detail/limb_algorithms.hpp> // limb kernels

#include <algorithm>
#include <bit>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <type_traits>

//---------------------------------------------------------------------------
// wide_int — a fixed-width two's-complement integer of N limbs, for raws and
// intermediates wider than 64 bits. It behaves like a builtin integer of
// N·limb_bits bits: + − × wrap, / and % truncate toward zero, >> is arithmetic
// when signed, and conversions to and from builtin integers truncate or
// sign-extend as the builtin conversions do. Unlike a builtin, overflow and
// out-of-range shifts are defined (wrap, and 0 or the sign fill).
//
// Structural (a public C array) and trivially copyable, so it can be an NTTP
// member and an inside raw. Every operation is constexpr and allocation-free.
//
// The limb type is a template parameter so tests can run wide_int over uint8_t
// limbs exhaustively against builtin oracles; the library uses umax limbs.
// The limb kernels below are also used by the C++26 big_rational.
//---------------------------------------------------------------------------
namespace beman::inside::detail {

template <std::size_t N, bool Signed, std::unsigned_integral L = umax>
struct wide_int {
    static_assert(N >= 1, "wide_int: at least one limb");
    static constexpr int  limb_bits = limb::bits<L>;
    static constexpr int  bits      = static_cast<int>(N) * limb_bits;
    static constexpr bool is_signed = Signed;

    L Word[N]; // little-endian limbs

    wide_int() = default;

    // From a builtin integer: sign-extends a negative signed value, as the
    // builtin conversions do.
    template <std::integral T>
    constexpr wide_int(T v) noexcept {
#if BEMAN_INSIDE_HAS_INT128
        using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
        using W = std::uint64_t;
#endif
        using S             = std::make_signed_t<W>;
        constexpr int wbits = std::numeric_limits<W>::digits;
        const W       w     = std::is_signed_v<T> ? static_cast<W>(static_cast<S>(v)) : static_cast<W>(v);
        const L       fill  = (std::is_signed_v<T> && v < 0) ? static_cast<L>(~L{0}) : L{0};
        for (std::size_t i = 0; i < N; ++i)
            Word[i] = (static_cast<int>(i) * limb_bits < wbits)
                          ? static_cast<L>(w >> (static_cast<int>(i) * limb_bits))
                          : fill;
    }

    // Between widths: sign-extends a signed source, truncates a wider one.
    // Implicit only when it widens.
    template <std::size_t M, bool S2>
        requires(M != N || S2 != Signed)
    constexpr explicit(M >= N) wide_int(const wide_int<M, S2, L>& o) noexcept {
        // (One loop, not copy_n + fill: that costs the math rows an instruction.)
        const L fill = o.negative() ? static_cast<L>(~L{0}) : L{0};
        for (std::size_t i = 0; i < N; ++i)
            Word[i] = (i < M) ? o.Word[i] : fill;
    }

    [[nodiscard]] constexpr bool negative() const noexcept { return Signed && (Word[N - 1] >> (limb_bits - 1)) != 0; }

    [[nodiscard]] constexpr bool is_zero() const noexcept {
        for (std::size_t i = 0; i < N; ++i) // (not ranges::all_of: slower to constant-evaluate)
            if (Word[i] != 0)
                return false;
        return true;
    }

    constexpr explicit operator bool() const noexcept { return !is_zero(); }

    // To a builtin integer: keeps the low bits (two's complement), as the
    // builtin narrowing conversions do.
    template <std::integral T>
    constexpr explicit operator T() const noexcept {
#if BEMAN_INSIDE_HAS_INT128
        using W = std::conditional_t<(sizeof(T) > 8), unsigned __int128, std::uint64_t>;
#else
        using W = std::uint64_t;
#endif
        constexpr int wbits = std::numeric_limits<W>::digits;
        W             acc   = 0;
        for (std::size_t i = 0; i < N && static_cast<int>(i) * limb_bits < wbits; ++i)
            acc |= static_cast<W>(Word[i]) << (static_cast<int>(i) * limb_bits);
        if constexpr (bits < wbits)
            if (negative())
                acc |= ~W{0} << bits;
        return static_cast<T>(acc);
    }

    // Nearest double (ties to even).
    constexpr explicit operator double() const noexcept {
        if (negative())
            return -static_cast<double>(wide_int<N, false, L>(-*this)); // −min reads as 2^(bits−1)
        const int bw = bit_width_of(*this);
        if (bw <= 64)
            return static_cast<double>(static_cast<std::uint64_t>(*this));
        // Keep the top 64 bits and fold the rest into a sticky bit: 64 > 53 + 2,
        // so the one uint64 → double rounding is the correct one.
        const int     sh  = bw - 64;
        std::uint64_t top = static_cast<std::uint64_t>(lshr(*this, sh));
        if (!(lshr(shl(*this, bits - sh), bits - sh)).is_zero())
            top |= 1u;
        return ldexp(static_cast<double>(top), sh);
    }

    // ---- arithmetic (wraps modulo 2^bits) -------------------------------
    // + − × stay written out: through limb::add_n / sub_n and a truncated
    // addmul_1 product the math tier's rows lose an instruction each (the
    // ins/op bench).
    friend constexpr wide_int operator+(wide_int a, const wide_int& b) noexcept {
        L c = 0;
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] = limb::add_carry(a.Word[i], b.Word[i], c);
        return a;
    }
    friend constexpr wide_int operator-(wide_int a, const wide_int& b) noexcept {
        L br = 0;
        for (std::size_t i = 0; i < N; ++i)
            a.Word[i] = limb::sub_borrow(a.Word[i], b.Word[i], br);
        return a;
    }
    friend constexpr wide_int operator-(const wide_int& a) noexcept { return wide_int{0} - a; }
    friend constexpr wide_int operator+(const wide_int& a) noexcept { return a; }

    friend constexpr wide_int operator*(const wide_int& a, const wide_int& b) noexcept {
        wide_int r{0};
        for (std::size_t i = 0; i < N; ++i) {
            if constexpr (N > 2) // a small value's high limbs: nothing to add
                if (a.Word[i] == 0)
                    continue;
            L carry = 0;
            for (std::size_t j = 0; i + j < N; ++j) {
                const limb::pair<L> p  = limb::mul(a.Word[i], b.Word[j]);
                L                   c1 = 0, c2 = 0;
                L                   s = limb::add_carry(r.Word[i + j], p.Lo, c1);
                s                     = limb::add_carry(s, carry, c2);
                r.Word[i + j]         = s;
                carry                 = static_cast<L>(p.Hi + c1 + c2); // fits: (b−1)² + 2(b−1) < b²
            }
        }
        return r;
    }

    struct divmod_result;
    // Truncating division, as the builtin operators. Division by zero fails
    // the build in constant evaluation and reports errc::division_by_zero at
    // runtime.
    [[nodiscard]] static constexpr divmod_result divmod(const wide_int& a, const wide_int& b) noexcept;

    friend constexpr wide_int operator/(const wide_int& a, const wide_int& b) noexcept {
        return divmod(a, b).Quotient;
    }
    friend constexpr wide_int operator%(const wide_int& a, const wide_int& b) noexcept {
        return divmod(a, b).Remainder;
    }

    // ---- bitwise ---------------------------------------------------------
    friend constexpr wide_int operator~(wide_int a) noexcept {
        std::ranges::transform(a.Word, a.Word, [](L w) { return static_cast<L>(~w); });
        return a;
    }
    friend constexpr wide_int operator&(wide_int a, const wide_int& b) noexcept {
        std::ranges::transform(a.Word, b.Word, a.Word, [](L x, L y) { return static_cast<L>(x & y); });
        return a;
    }
    friend constexpr wide_int operator|(wide_int a, const wide_int& b) noexcept {
        std::ranges::transform(a.Word, b.Word, a.Word, [](L x, L y) { return static_cast<L>(x | y); });
        return a;
    }
    friend constexpr wide_int operator^(wide_int a, const wide_int& b) noexcept {
        std::ranges::transform(a.Word, b.Word, a.Word, [](L x, L y) { return static_cast<L>(x ^ y); });
        return a;
    }

    // Shifts by s >= bits give 0 (<<) or the sign fill (>>); negative s is a
    // precondition violation, as for builtins.
    friend constexpr wide_int operator<<(const wide_int& a, int s) noexcept { return shl(a, s); }
    friend constexpr wide_int operator>>(const wide_int& a, int s) noexcept {
        const L fill = a.negative() ? static_cast<L>(~L{0}) : L{0};
        return shr(a, s, fill);
    }

    // ---- comparison --------------------------------------------------------
    // Written out: GCC 15/16 miscompare a defaulted == over an array member in
    // constant evaluation (`a == x && b == y` can come out true with b != y).
    friend constexpr bool operator==(const wide_int& a, const wide_int& b) noexcept {
        for (std::size_t i = 0; i < N; ++i) // (not ranges::equal: that calls memcmp)
            if (a.Word[i] != b.Word[i])
                return false;
        return true;
    }
    // (A loop, not lexicographical_compare_three_way on reverse iterators,
    // which moves the ins/op bench both ways.)
    friend constexpr std::strong_ordering operator<=>(const wide_int& a, const wide_int& b) noexcept {
        if (a.negative() != b.negative())
            return a.negative() ? std::strong_ordering::less : std::strong_ordering::greater;
        for (std::size_t i = N; i-- > 0;)
            if (a.Word[i] != b.Word[i])
                return a.Word[i] <=> b.Word[i];
        return std::strong_ordering::equal;
    }

    // ---- compound forms ----------------------------------------------------
    constexpr wide_int& operator+=(const wide_int& b) noexcept { return *this = *this + b; }
    constexpr wide_int& operator-=(const wide_int& b) noexcept { return *this = *this - b; }
    constexpr wide_int& operator*=(const wide_int& b) noexcept { return *this = *this * b; }
    constexpr wide_int& operator/=(const wide_int& b) noexcept { return *this = *this / b; }
    constexpr wide_int& operator%=(const wide_int& b) noexcept { return *this = *this % b; }
    constexpr wide_int& operator&=(const wide_int& b) noexcept { return *this = *this & b; }
    constexpr wide_int& operator|=(const wide_int& b) noexcept { return *this = *this | b; }
    constexpr wide_int& operator^=(const wide_int& b) noexcept { return *this = *this ^ b; }
    constexpr wide_int& operator<<=(int s) noexcept { return *this = *this << s; }
    constexpr wide_int& operator>>=(int s) noexcept { return *this = *this >> s; }
    constexpr wide_int& operator++() noexcept { return *this += wide_int{1}; }
    constexpr wide_int& operator--() noexcept { return *this -= wide_int{1}; }
    constexpr wide_int  operator++(int) noexcept {
        wide_int t = *this;
        ++*this;
        return t;
    }
    constexpr wide_int operator--(int) noexcept {
        wide_int t = *this;
        --*this;
        return t;
    }

    // |a| (a signed minimum stays itself, as the builtin negation wraps).
    [[nodiscard]] friend constexpr wide_int abs(const wide_int& a) noexcept { return a.negative() ? -a : a; }

    // Number of significant bits of a non-negative value (0 for 0).
    [[nodiscard]] friend constexpr int bit_width_of(const wide_int& a) noexcept {
        const std::size_t n = limb::top_size(a.Word, N);
        return n == 0 ? 0 : static_cast<int>(n - 1) * limb_bits + static_cast<int>(std::bit_width(a.Word[n - 1]));
    }

  private:
    static constexpr wide_int shl(const wide_int& a, int s) noexcept {
        wide_int r{0};
        if (s < bits)
            limb::shift_left(r.Word, N, a.Word, N, s);
        return r;
    }
    static constexpr wide_int shr(const wide_int& a, int s, L fill) noexcept {
        wide_int r;
        if (s >= bits)
            for (std::size_t i = 0; i < N; ++i) // (not ranges::fill: that calls memset)
                r.Word[i] = fill;
        else
            limb::shift_right(r.Word, N, a.Word, N, s, fill);
        return r;
    }
    static constexpr wide_int lshr(const wide_int& a, int s) noexcept { return shr(a, s, L{0}); }
};

template <std::size_t N, bool Signed, std::unsigned_integral L>
struct wide_int<N, Signed, L>::divmod_result {
    wide_int Quotient;
    wide_int Remainder;
};

template <std::size_t N, bool Signed, std::unsigned_integral L>
constexpr auto wide_int<N, Signed, L>::divmod(const wide_int& a, const wide_int& b) noexcept -> divmod_result {
    if (b.is_zero())
        fail_hard<"wide_int: division by zero">(errc::division_by_zero);
    // Divide the magnitudes; −min reads correctly as an unsigned magnitude.
    const bool        na = a.negative(), nb = b.negative();
    const wide_int    ua = na ? -a : a, ub = nb ? -b : b;
    const std::size_t m = limb::top_size(ua.Word, N);
    std::size_t       n = N; // b ≠ 0: no n > 0 test, which GCC's bounds warnings would need
    while (ub.Word[n - 1] == 0)
        --n;
    divmod_result res{wide_int{0}, wide_int{0}};
    if (m < n)
        res.Remainder = ua;
    else {
        L un[N + 1]{}, vn[N]{};
        limb::divmod(ua.Word, m, ub.Word, n, res.Quotient.Word, res.Remainder.Word, un, vn);
    }
    if (na != nb)
        res.Quotient = -res.Quotient;
    if (na)
        res.Remainder = -res.Remainder;
    return res;
}

template <std::size_t N, std::unsigned_integral L = umax>
using wide_uint = wide_int<N, false, L>;
template <std::size_t N, std::unsigned_integral L = umax>
using wide_sint = wide_int<N, true, L>;

// n / d for a one-limb d: the quotient and the remainder.
template <std::size_t K>
struct small_divmod {
    wide_uint<K> Quotient;
    umax         Remainder;
};

template <std::size_t K>
constexpr small_divmod<K> divmod_small(const wide_uint<K>& n, umax d) noexcept {
    small_divmod<K> r{wide_uint<K>{0}, 0};
    for (std::size_t i = K; i-- > 0;) { // (not limb::div_1: slower to constant-evaluate)
        const limb::pair<umax> qr = limb::div(r.Remainder, n.Word[i], d);
        r.Quotient.Word[i]        = qr.Hi;
        r.Remainder               = qr.Lo;
    }
    return r;
}

template <typename T>
inline constexpr bool is_wide_int_v = false;
template <std::size_t N, bool S, std::unsigned_integral L>
inline constexpr bool is_wide_int_v<wide_int<N, S, L>> = true;

// A wide_int of any width and signedness.
template <typename T>
concept wide_integer = is_wide_int_v<std::remove_cvref_t<T>>;

// An integer the library may hold in a raw or intermediate: a builtin
// integer or a wide_int. (std::integral cannot be extended to wide_int.)
template <typename T>
concept raw_integer = std::integral<T> || wide_integer<T>;

// gcd of two non-negative wide integers (Euclid; 0 when both are).
template <wide_integer I>
constexpr I wide_gcd(I a, I b) noexcept {
    return euclid_gcd(a, b);
}

//---------------------------------------------------------------------------
// wide_rational — an exact value as a fraction of K-limb integers: the exact
// paths' intermediate (wide_value.hpp), and, reduced, the raw of a continuous
// grid whose limits pass 64 bits. Structural, so it can be a raw.
//---------------------------------------------------------------------------
inline constexpr std::size_t exact_min_limbs = 8; // scalars, 64-bit rationals

template <std::size_t K>
struct wide_rational {
    wide_sint<K> Num{0};
    wide_sint<K> Den{1}; // > 0; not reduced

    constexpr wide_rational() = default;
    constexpr wide_rational(wide_sint<K> n, wide_sint<K> d) noexcept : Num{n}, Den{d} {}
    template <std::size_t M>
        requires(M < K)
    constexpr wide_rational(const wide_rational<M>& o) noexcept : Num{o.Num}, Den{o.Den} {} // widens

    constexpr explicit operator double() const noexcept { return static_cast<double>(Num) / static_cast<double>(Den); }
};

template <typename T>
inline constexpr bool is_wide_rational_v = false;
template <std::size_t K>
inline constexpr bool is_wide_rational_v<wide_rational<K>> = true;

// A wide_rational of any width.
template <typename T>
concept wide_rational_like = is_wide_rational_v<std::remove_cvref_t<T>>;
} // namespace beman::inside::detail

template <std::size_t N, bool Signed, std::unsigned_integral L>
struct std::numeric_limits<beman::inside::detail::wide_int<N, Signed, L>> {
  private:
    using W = beman::inside::detail::wide_int<N, Signed, L>;
    static constexpr W top_bit() noexcept { return W{1} << (W::bits - 1); }

  public:
    static constexpr bool is_specialized    = true;
    static constexpr bool is_signed         = Signed;
    static constexpr bool is_integer        = true;
    static constexpr bool is_exact          = true;
    static constexpr bool has_infinity      = false;
    static constexpr bool has_quiet_NaN     = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool is_bounded        = true;
    static constexpr bool is_modulo         = !Signed;
    static constexpr int  radix             = 2;
    static constexpr int  digits            = W::bits - (Signed ? 1 : 0);
    static constexpr int  digits10          = digits * 30103 / 100000; // ⌊digits·log10 2⌋
    static constexpr W    min() noexcept { return Signed ? top_bit() : W{0}; }
    static constexpr W    lowest() noexcept { return min(); }
    static constexpr W    max() noexcept { return Signed ? ~top_bit() : ~W{0}; }
};

BEMAN_INSIDE_WARNINGS_POP

#endif // BEMAN_INSIDE_DETAIL_WIDE_INT_HPP

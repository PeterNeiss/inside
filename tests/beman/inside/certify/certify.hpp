// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//---------------------------------------------------------------------------
// certify — a proof by exhaustion for In → Out pairs of the math functions.
//
// For every input of a pair it calls the public function twice: as any
// run-time call (the double and dd tiers first), and with
// math::driver.IntegerOnly, which computes what constant evaluation and a
// BEMAN_INSIDE_MATH_NO_FP build compute. It counts the inputs where the two
// differ and the integer path's guesses (results still undecided at the
// precision cap), and hashes every result. A pair with no difference and no
// guess is certified: every result is the correctly rounded point and the
// same in every build. The hash lets anyone check that on another machine.
//
// A program lists its pairs and hands them to certify::main:
//
//   #include "certify.hpp"            // before any beman.inside header
//   void pairs(beman::inside::certify::runner& r) {
//       r.unary<Out, In>("sin", [](In x) { return beman::inside::math::sin_into<Out>(x); });
//   }
//   int main(int argc, char** argv) { return beman::inside::certify::main(argc, argv, pairs); }
//
// find_grids.py writes such a program from the pairs a build uses. Options:
//   --estimate        list the pairs and the estimated time, then stop
//   --max-slots N     skip pairs with more inputs (default 2^32; a debug build caps it at 4096)
//   --max-seconds S   skip pairs estimated to take longer
//   --pair TEXT       only the pairs whose "function input output" contains TEXT
//   --markdown FILE   also write the table as a page (docs/certified.md)
//   --check FILE      fail unless every hash equals the one in FILE's table
//   --require-all     with --check: a skipped pair must have a row in FILE too
//   --dump            print every result record of the selected pairs
//   --progress / --no-progress   progress on stderr (default: when it is a terminal)
//
// The hash: SHA-256 over one 17-byte record per input, in input order (two
// inputs: the first argument's slots outer). A record is a kind byte (0 for
// a value ≥ 0, 1 for a negative value, 2 for an errc), then the reduced
// value's numerator magnitude and its denominator, each 8 bytes little
// endian; an errc record holds the code, then 0.
//---------------------------------------------------------------------------
#ifndef BEMAN_INSIDE_CERTIFY_HPP
#define BEMAN_INSIDE_CERTIFY_HPP

#if defined(BEMAN_INSIDE_DETAIL_CONFIG_HPP)
    #error "certify.hpp must be included before every beman.inside header"
#endif
#define BEMAN_INSIDE_MATH_INSTRUMENT 1
#define BEMAN_INSIDE_MATH_TABLE_SLOTS 0 // every call computes, so the integer path's counters see it

#include <beman/inside/cmath.hpp>
#include <beman/inside/io.hpp>

#include "sha256.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#if defined(_WIN32)
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace beman::inside::certify {

// A grid from its fields' raw parts (numerator magnitude, signed
// denominator), as a demangled symbol spells them: find_grids.py's form.
constexpr grid
grid_from(umax lo_n, imax lo_d, umax hi_n, imax hi_d, umax notch_n, imax notch_d, umax max_n = 0, imax max_d = 1) {
    using detail::make_raw;
    grid g{interval{make_raw(lo_n, lo_d), make_raw(hi_n, hi_d)}, make_raw(notch_n, notch_d)};
    g.MaxDenominator = make_raw(max_n, max_d);
    return g;
}

// "{[-4, 4], 1/4} round_nearest, clamp": the grid's limits and notch as
// fractions, and the policy's flags by name.
inline std::string number_text(const detail::grid_rational& v) {
#if BEMAN_INSIDE_BIG_GRIDS
    // A grid number of any size, as the 64-bit form prints it.
    const std::string n = to_string(v.Num);
    return v.is_integer() ? n : n + "/" + to_string(v.Den);
#else
    const std::string n = (v.Denominator < 0 && v.Numerator != 0 ? "-" : "") + std::to_string(v.Numerator);
    const umax        d = detail::abs_den(v.Denominator);
    return d == 1 ? n : n + "/" + std::to_string(d);
#endif
}
inline std::string grid_text(const grid& g) {
    std::string out =
        "{[" + number_text(g.Interval.Lower) + ", " + number_text(g.Interval.Upper) + "], " + number_text(g.Notch);
    if (g.MaxDenominator != 0)
        out += ", den ≤ " + number_text(g.MaxDenominator);
    return out + "}";
}
inline std::string flags_text(policy_flag f) {
    std::string out;
    auto        add = [&](const char* name) {
        if (!out.empty())
            out += ", ";
        out += name;
    };
    if (has_flag(f, detail::unsafe_marker)) {
        add("unsafe");
        f &= ~unsafe;
    }
    if (has_flag(f, round_half_even))
        add("round_half_even");
    else if (has_flag(f, round_ceil))
        add("round_ceil");
    else if (has_flag(f, round_floor))
        add("round_floor");
    else if (has_flag(f, round_nearest))
        add("round_nearest");
    else if (has_flag(f, snap))
        add("round_toward_zero");
    f &= ~(round_half_even | round_ceil | round_floor | round_nearest | snap);
    for (const auto& [flag, name] : {std::pair{clamp, "clamp"},
                                     std::pair{wrap, "wrap"},
                                     std::pair{checked, "checked"},
                                     std::pair{ignore_zero, "ignore_zero"},
                                     std::pair{ignore_range, "ignore_range"}})
        if (has_flag(f, flag)) {
            add(name);
            f &= ~flag;
        }
    if (f != 0) {
        char hex[32];
        std::snprintf(hex, sizeof hex, "0x%llx", static_cast<unsigned long long>(f));
        add(hex);
    }
    return out.empty() ? std::string{"none"} : out;
}
template <insidable T>
std::string type_text() {
    return grid_text(grid_of<T>) + " " + flags_text(flags_of<T>);
}

// One result: a value as a reduced fraction, or an errc.
struct record {
    unsigned char Kind      = 0; // 0: ≥ 0, 1: negative, 2: errc
    std::uint64_t Magnitude = 0, Denominator = 0;
    friend bool   operator==(const record&, const record&) = default;
};

template <class R>
record record_of(const R& r) {
    if constexpr (requires { r.has_value(); }) {
        if (!r.has_value())
            return {2, static_cast<std::uint64_t>(r.error()), 0};
        return record_of(*r);
    } else {
        const rational      v{r};
        std::uint64_t       n = v.Numerator;
        std::uint64_t       d = detail::abs_den(v.Denominator);
        const std::uint64_t g = std::gcd(n, d);
        n /= g;
        d /= g;
        return {static_cast<unsigned char>(v.Denominator < 0 && n != 0 ? 1 : 0), n, n == 0 ? 1 : d};
    }
}

inline void hash_record(sha256& h, const record& r) {
    unsigned char bytes[17];
    bytes[0] = r.Kind;
    for (int i = 0; i < 8; ++i) {
        bytes[1 + i] = static_cast<unsigned char>(r.Magnitude >> (8 * i));
        bytes[9 + i] = static_cast<unsigned char>(r.Denominator >> (8 * i));
    }
    h.update(bytes, sizeof bytes);
}

inline std::string record_text(const record& r) {
    if (r.Kind == 2)
        return "errc " + std::to_string(r.Magnitude);
    return (r.Kind == 1 ? "-" : "") + std::to_string(r.Magnitude) + "/" + std::to_string(r.Denominator);
}

// The input with slot offset i.
// The number of inputs, saturating at 2^64 − 1 (a pair that large is skipped).
constexpr std::uint64_t saturated = ~std::uint64_t{0};
template <insidable In>
std::uint64_t slots_of() {
    constexpr auto count = grid_of<In>.slot_count();
    if constexpr (count < detail::grid_int{saturated})
        return static_cast<std::uint64_t>(count) + 1;
    else
        return saturated;
}
constexpr std::uint64_t product(std::uint64_t a, std::uint64_t b) {
    return b != 0 && a > saturated / b ? saturated : a * b;
}
template <insidable In>
In input(std::uint64_t i) {
    return In::from_raw(detail::raw_from_offset<In>(static_cast<umax>(i)));
}

// Input i, computed as a run-time call and on the integer path.
using evaluator = std::function<void(std::uint64_t, record&, record&)>;

template <class Fn, class... X>
void evaluate_both(const Fn& fn, record& fast, record& integer, const X&... x) {
    math::driver.IntegerOnly = false;
    fast                     = record_of(fn(x...));
    math::driver.IntegerOnly = true;
    integer                  = record_of(fn(x...));
    math::driver.IntegerOnly = false;
}

struct job {
    std::string   Section, Function, In, Out;
    std::uint64_t Slots = 0;
    evaluator     Eval;
    std::string   key() const { return Function + " " + In + " " + Out; }
};

struct result {
    const job*    Job        = nullptr;
    std::uint64_t Mismatches = 0, Deep = 0, Guesses = 0;
    std::string   Hash, Note;
    double        Seconds = 0, Estimate = 0;
    bool          Ran = false;
    bool          ok() const { return !Ran || (Mismatches == 0 && Guesses == 0); }
};

struct options {
    std::uint64_t MaxSlots   = std::uint64_t{1} << 32; // hours at most
    double        MaxSeconds = 0;                      // 0: no limit
    std::string   Pair, Markdown, Check;
    bool          Estimate = false, Dump = false, RequireAll = false;
    int           Progress = -1; // -1: when stderr is a terminal
};

class runner {
  public:
    // The pairs added after this go under its Markdown heading and text.
    void section(std::string heading) { Section = std::move(heading); }

    template <insidable Out, insidable In, class Fn>
    void unary(std::string function, Fn fn) {
        Jobs.push_back({Section,
                        std::move(function),
                        type_text<In>(),
                        type_text<Out>(),
                        slots_of<In>(),
                        [fn](std::uint64_t i, record& fast, record& integer) {
                            evaluate_both(fn, fast, integer, input<In>(i));
                        }});
    }
    // Two inputs: every pair of slots, A's outer.
    template <insidable Out, insidable A, insidable B, class Fn>
    void binary(std::string function, Fn fn) {
        const std::uint64_t nb = slots_of<B>();
        Jobs.push_back({Section,
                        std::move(function),
                        type_text<A>() + " × " + type_text<B>(),
                        type_text<Out>(),
                        product(slots_of<A>(), nb),
                        [fn, nb](std::uint64_t i, record& fast, record& integer) {
                            evaluate_both(fn, fast, integer, input<A>(i / nb), input<B>(i % nb));
                        }});
    }

    const std::vector<job>& jobs() const { return Jobs; }

  private:
    std::string      Section;
    std::vector<job> Jobs;
};

//---------------------------------------------------------------------------
// Text helpers
//---------------------------------------------------------------------------
inline std::string duration_text(double s) {
    char buf[64];
    if (s < 1)
        return "<1 s";
    if (s < 60)
        std::snprintf(buf, sizeof buf, "%.0f s", s);
    else if (s < 3600)
        std::snprintf(buf, sizeof buf, "%d min %02d s", static_cast<int>(s) / 60, static_cast<int>(s) % 60);
    else if (s < 86400 * 2)
        std::snprintf(buf, sizeof buf, "%d h %02d min", static_cast<int>(s / 3600), static_cast<int>(s / 60) % 60);
    else if (s < 86400 * 365)
        std::snprintf(buf, sizeof buf, "%.1f days", s / 86400);
    else
        std::snprintf(buf, sizeof buf, "%.2g years", s / (86400 * 365.25));
    return buf;
}
inline std::string count_text(std::uint64_t n) {
    char buf[32];
    if (n == saturated)
        return "> 1.8e+19";
    if (n >= 1'000'000'000'000)
        std::snprintf(buf, sizeof buf, "%.1e", static_cast<double>(n));
    else if (n >= 1'000'000'000)
        std::snprintf(buf, sizeof buf, "%.1f G", static_cast<double>(n) / 1e9);
    else if (n >= 1'000'000)
        std::snprintf(buf, sizeof buf, "%.1f M", static_cast<double>(n) / 1e6);
    else if (n >= 10'000)
        std::snprintf(buf, sizeof buf, "%.1f k", static_cast<double>(n) / 1e3);
    else
        std::snprintf(buf, sizeof buf, "%llu", static_cast<unsigned long long>(n));
    return buf;
}

inline bool stderr_is_terminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

//---------------------------------------------------------------------------
// Estimate, sweep, report
//---------------------------------------------------------------------------
using steady = std::chrono::steady_clock;
inline double seconds_since(steady::time_point t) { return std::chrono::duration<double>(steady::now() - t).count(); }

// Seconds per input, from up to 2048 inputs spread evenly over the grid
// (the cost varies with x).
inline double calibrate(const job& j) {
    const std::uint64_t n = std::min<std::uint64_t>(j.Slots, 2048);
    record              fast, integer;
    const auto          start = steady::now();
    for (std::uint64_t i = 0; i < n; ++i)
        j.Eval(i * (j.Slots / n), fast, integer);
    return seconds_since(start) / static_cast<double>(n);
}

struct progress {
    bool               On = false;
    steady::time_point Last{};
    void show(std::size_t index, std::size_t count, const job& j, std::uint64_t done, double elapsed, double later) {
        if (!On || seconds_since(Last) < 1)
            return;
        Last              = steady::now();
        const double rate = static_cast<double>(done) / std::max(elapsed, 1e-9);
        const double left = static_cast<double>(j.Slots - done) / std::max(rate, 1e-9);
        std::fprintf(stderr,
                     "\r[%zu/%zu] %s  %3.0f%%  %s/s  ETA %s (all: %s)   ",
                     index + 1,
                     count,
                     j.Function.c_str(),
                     100.0 * static_cast<double>(done) / static_cast<double>(j.Slots),
                     count_text(static_cast<std::uint64_t>(rate)).c_str(),
                     duration_text(left).c_str(),
                     duration_text(left + later).c_str());
        std::fflush(stderr);
    }
    void clear() const {
        if (On)
            std::fprintf(stderr, "\r%*s\r", 100, "");
    }
};

inline void
sweep(const job& j, result& r, const options& o, progress& p, std::size_t index, std::size_t count, double later) {
    math::driver.Deep    = 0;
    math::driver.Guesses = 0;
    sha256     h;
    record     fast, integer;
    const auto start = steady::now();
    if (o.Dump)
        std::printf("# %s %s -> %s\n", j.Function.c_str(), j.In.c_str(), j.Out.c_str());
    for (std::uint64_t i = 0; i < j.Slots; ++i) {
        j.Eval(i, fast, integer);
        if (!(fast == integer) && r.Mismatches++ < 3)
            std::fprintf(stderr,
                         "\n  %s: input %llu differs: %s, integer path %s\n",
                         j.Function.c_str(),
                         static_cast<unsigned long long>(i),
                         record_text(fast).c_str(),
                         record_text(integer).c_str());
        hash_record(h, fast);
        if (o.Dump)
            std::printf("%s\n", record_text(fast).c_str());
        if ((i & 0xfff) == 0xfff)
            p.show(index, count, j, i + 1, seconds_since(start), later);
    }
    r.Seconds = seconds_since(start);
    r.Deep    = math::driver.Deep.load();
    r.Guesses = math::driver.Guesses.load();
    r.Hash    = h.hex();
    r.Ran     = true;
    if (o.Dump)
        std::printf("# sha256 %s\n", r.Hash.c_str());
}

// The table: one row per pair, a new table under each section's heading.
// `|` never appears in a cell (flags are comma-separated), so a row splits
// on it.
inline std::string table(const std::vector<result>& results) {
    std::string out;
    for (std::size_t i = 0; i < results.size(); ++i) {
        const job&    j = *results[i].Job;
        const result& r = results[i];
        if (i == 0 || j.Section != results[i - 1].Job->Section)
            out +=
                (i == 0 ? "" : "\n") + (j.Section.empty() ? "" : j.Section + "\n\n") +
                "| function | input | output | inputs | guesses | fp ≠ integer | SHA-256 of the results | verdict |\n"
                "|:--|:--|:--|--:|--:|--:|:--|:--|\n";
        out += "| `" + j.Function + "` | `" + j.In + "` | `" + j.Out + "` | " +
               (j.Slots == saturated ? count_text(j.Slots) : std::to_string(j.Slots)) + " | ";
        if (r.Ran)
            out += std::to_string(r.Guesses) + " | " + std::to_string(r.Mismatches) + " | `" + r.Hash + "` | " +
                   (r.ok() ? "certified" : "**failed**") + " |\n";
        else
            out += "– | – | – | skipped: " + r.Note + " |\n";
    }
    return out;
}

// The rows of FILE's tables, by "function input output": the committed hash,
// or empty for a row without one (a skipped pair).
inline std::map<std::string, std::string> read_hashes(const std::string& file) {
    std::map<std::string, std::string> hashes;
    std::ifstream                      in{file};
    std::string                        line;
    while (std::getline(in, line)) {
        std::vector<std::string> cells;
        std::stringstream        ss{line};
        std::string              cell;
        while (std::getline(ss, cell, '|')) {
            const auto a = cell.find_first_not_of(" `"), b = cell.find_last_not_of(" `");
            cells.push_back(a == std::string::npos ? std::string{} : cell.substr(a, b - a + 1));
        }
        // "", function, input, output, inputs, guesses, mismatches, hash, verdict
        if (cells.size() >= 9 && cells[1] != "function" && !cells[1].starts_with(":--"))
            hashes[cells[1] + " " + cells[2] + " " + cells[3]] = cells[7].size() == 64 ? cells[7] : "";
    }
    return hashes;
}

inline const char* page_header =
    "# Certified math pairs\n"
    "\n"
    "<!--\nSPDX-License-Identifier: Apache-2.0 WITH LLVM-exception\n-->\n"
    "\n"
    "Generated by `beman.inside.certify` (tests/beman/inside/certify.test.cpp) — do not edit by hand.\n"
    "Regenerate with `cmake --build <build-dir> --target beman.inside.certify_report` (an optimized build).\n"
    "\n"
    "Every input of each pair was computed twice, as a run-time call and on the integer path alone (what\n"
    "constant evaluation and a `BEMAN_INSIDE_MATH_NO_FP` build compute). A certified pair has no input where\n"
    "the two differ and no result left undecided at the engine's precision cap: every result is the correctly\n"
    "rounded point, the same in every build. The hash covers every result, so a build on another compiler,\n"
    "platform or FPU reproduces it exactly. Every CI configuration checks the pairs up to 2^20 inputs, and\n"
    "the weekly run all of them.\n"
    "[certify.md](certify.md) explains the tool, the hash, and how to certify the grids of your own program.\n"
    "\n";

inline int main(int argc, char** argv, void (*pairs)(runner&)) {
    options o;
    for (int i = 1; i < argc; ++i) {
        const std::string_view a    = argv[i];
        auto                   next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--max-slots")
            o.MaxSlots = std::strtoull(next(), nullptr, 10);
        else if (a == "--max-seconds")
            o.MaxSeconds = std::strtod(next(), nullptr);
        else if (a == "--pair")
            o.Pair = next();
        else if (a == "--markdown")
            o.Markdown = next();
        else if (a == "--check")
            o.Check = next();
        else if (a == "--require-all")
            o.RequireAll = true;
        else if (a == "--estimate")
            o.Estimate = true;
        else if (a == "--dump")
            o.Dump = true;
        else if (a == "--progress")
            o.Progress = 1;
        else if (a == "--no-progress")
            o.Progress = 0;
        else {
            std::fprintf(stderr,
                         "usage: %s [--estimate] [--max-slots N] [--max-seconds S] [--pair TEXT]\n"
                         "          [--markdown FILE] [--check FILE] [--require-all] [--dump]\n"
                         "          [--progress | --no-progress]\n",
                         argv[0]);
            return 2;
        }
    }

#ifndef NDEBUG
    o.MaxSlots = std::min<std::uint64_t>(o.MaxSlots, 4096); // unoptimized: the small pairs only
#endif
    runner r;
    pairs(r);
    std::vector<result> results;
    for (const job& j : r.jobs())
        if (o.Pair.empty() || j.key().find(o.Pair) != std::string::npos) {
            result res;
            res.Job = &j;
            results.push_back(res);
        }

    // The plan: each pair's size and estimated time.
    double        total  = 0;
    std::uint64_t inputs = 0;
    std::size_t   to_run = 0;
    for (result& res : results) {
        const job& j = *res.Job;
        res.Estimate = calibrate(j) * static_cast<double>(j.Slots);
        if (j.Slots > o.MaxSlots) {
            res.Note = count_text(j.Slots) + " inputs, over --max-slots (~" + duration_text(res.Estimate) + ")";
            continue;
        }
        if (o.MaxSeconds > 0 && res.Estimate > o.MaxSeconds) {
            res.Note = "~" + duration_text(res.Estimate) + ", over --max-seconds";
            continue;
        }
        total += res.Estimate;
        inputs += j.Slots;
        ++to_run;
    }
    std::FILE* plan = o.Estimate ? stdout : stderr;
    std::fprintf(plan,
                 "certify: %zu of %zu pairs, %s inputs, about %s\n",
                 to_run,
                 results.size(),
                 count_text(inputs).c_str(),
                 duration_text(total).c_str());
    for (const result& res : results)
        std::fprintf(plan,
                     "  %-12s %10s inputs  %18s  %s -> %s\n",
                     res.Job->Function.c_str(),
                     count_text(res.Job->Slots).c_str(),
                     ((res.Note.empty() ? "~" : "skipped ~") + duration_text(res.Estimate)).c_str(),
                     res.Job->In.c_str(),
                     res.Job->Out.c_str());
    if (o.Estimate)
        return 0;

    progress    p{o.Progress < 0 ? stderr_is_terminal() : o.Progress == 1};
    double      estimated_left = total, measured = 0, estimated_done = 0;
    std::size_t index = 0;
    for (result& res : results) {
        if (!res.Note.empty())
            continue;
        estimated_left -= res.Estimate;
        // Later pairs' estimates, corrected by how the finished ones came out.
        const double scale = estimated_done > 0 ? measured / estimated_done : 1;
        sweep(*res.Job, res, o, p, index++, to_run, estimated_left * scale);
        p.clear();
        measured += res.Seconds;
        estimated_done += res.Estimate;
        std::fprintf(stderr,
                     "[%zu/%zu] %-9s %s in %s%s\n",
                     index,
                     to_run,
                     res.ok() ? "certified" : "FAILED",
                     res.Job->Function.c_str(),
                     duration_text(res.Seconds).c_str(),
                     estimated_left > 0
                         ? (", about " + duration_text(estimated_left * measured / estimated_done) + " left").c_str()
                         : "");
    }

    int status = 0;
    for (const result& res : results)
        status |= res.ok() ? 0 : 1;
    if (!o.Check.empty()) {
        const auto committed = read_hashes(o.Check);
        for (const result& res : results) {
            if (!res.Ran && !o.RequireAll)
                continue;
            const auto it = committed.find(res.Job->key());
            if (it == committed.end()) {
                std::fprintf(stderr, "certify: not in %s: %s\n", o.Check.c_str(), res.Job->key().c_str());
                status = 1;
            } else if (res.Ran && it->second != res.Hash) {
                std::fprintf(stderr,
                             "certify: hash differs from %s: %s\n  computed  %s\n  committed %s\n",
                             o.Check.c_str(),
                             res.Job->key().c_str(),
                             res.Hash.c_str(),
                             it->second.empty() ? "none (skipped there)" : it->second.c_str());
                status = 1;
            }
        }
    }
    const std::string t = table(results);
    if (!o.Dump)
        std::fputs(t.c_str(), stdout);
    if (!o.Markdown.empty()) {
        std::ofstream out{o.Markdown};
        out << page_header << t;
    }
    return status;
}

} // namespace beman::inside::certify

#endif

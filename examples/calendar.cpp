// Simplified calendar with day → month → year cascade via `on_wrap`.
// Months are 30 days for clarity; real calendars need leap-year handling.
//
// Demonstrates:
//   - Three-level on_wrap cascade (similar to clock.cpp but with year axis)
//   - `_ins` literal for compile-time constants (DAYS_PER_MONTH)
//   - Modulo `%` for day-of-week (under `snap`)

#include <iostream>

#include <beman/inside/inside.hpp>
#include <beman/inside/io.hpp>

using namespace beman::inside;

using day_t   = inside<{1, 30}, wrap>;       // 1-30, wraps to 1 after 30
using month_t = inside<{1, 12}, wrap>;       // 1-12, wraps to 1 after 12
using year_t  = inside<{1900, 2200}>;
using dow_t   = inside<{0, 6}, snap>; // 0=Sun..6=Sat (currently unused)

struct date
{
  day_t   day{1};
  month_t month{1};
  year_t  year{2000};

  // `n` is `numeric` (an inside delta or the inside carry), not `int`/`arithmetic`:
  // the inside-RHS `+=` makes the wrap carry an inside, so the day→month→year cascade
  // threads bounds, not raw integers.
  void add_days(numeric auto n)
  {
    day.on_wrap([&](auto&, auto carry) { add_months(carry); }) += n;
  }

  void add_months(numeric auto n)
  {
    month.on_wrap([&](auto&, auto carry) { year += carry; }) += n;
  }

  friend std::ostream& operator<<(std::ostream& os, const date& d)
  {
    return os << d.year << '-'
              << (d.month < 10 ? "0" : "") << d.month << '-'
              << (d.day   < 10 ? "0" : "") << d.day;
  }
};

int main()
{
  date d{};
  d.day = day_t{15};
  d.month = month_t{3};
  d.year = year_t{2026};
  std::cout << "start:       " << d << "\n";

  // _ins literal as a compile-time constant for the cycle length.
  constexpr auto days_per_month = 30_ins;
  std::cout << "(days per month constant: " << days_per_month << ")\n";

  d.add_days(20_ins);
  std::cout << "+20 days:    " << d << "\n";

  d.add_days(45_ins);
  std::cout << "+45 days:    " << d << "\n";

  d.add_months(11_ins);
  std::cout << "+11 months:  " << d << "\n";

  // Big jump that cascades all three axes.
  d.add_days(400_ins);
  std::cout << "+400 days:   " << d << "\n";

  // Day-of-week via modulo. Each calendar day maps to a weekday in [0, 6].
  // Sample the next 10 days from a known Sunday reference.
  using big_day_t = inside<{0, 1000000}, snap>;
  using week_size = inside<{1, 7},       snap>;
  constexpr week_size seven{7};

  std::cout << "\nday-of-week sequence (0=Sun..6=Sat):\n";
  for (int n = 0; n < 10; ++n)
  {
    big_day_t ordinal{14000 + n};
    // `seven` has grid {1,7}, which excludes zero, so `%` cannot be
    // division-by-zero: the result is a plain inside, no optional to unwrap.
    auto dow = ordinal % seven;
    std::cout << "  day " << ordinal << " -> dow " << dow << "\n";
  }

  return 0;
}

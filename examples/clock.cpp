// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Carry cascades with wrap and on_wrap: a field that overflows hands the carry to the next one up,
// one line per field.
// Each field is a quantity in its own unit, so a carry of n periods is just n of the next unit.

#include <array>
#include <iostream>

#include <inside/inside.hpp>
#include <inside/inside/io.hpp>
#include <inside/units.hpp>
#include <inside/units/io.hpp>

using namespace ins;

namespace
{
  // Calendar units of our own: a 30-day month (for clarity), a 12-month year, a 7-day week.
  inline constexpr auto month = unit<"mo", just<30> * si::day>;
  inline constexpr auto year  = unit<"yr", just<12> * month>;
  inline constexpr auto week  = unit<"wk", just<7>  * si::day>;
} // namespace

// A 24-hour clock: seconds → minutes → hours, wrapping at 24.
struct clock24
{
  using hours_t   = quantity<si::hour  , inside<{0, 24, bounds::half_open}, wrap>>;
  using minutes_t = quantity<si::minute, inside<{0, 60, bounds::half_open}, wrap>>;
  using seconds_t = quantity<si::second, inside<{0, 60, bounds::half_open}, wrap>>;

  hours_t   hours  {0, si::hour};
  minutes_t minutes{0, si::minute};
  seconds_t seconds{0, si::second};

  clock24() = default;
  clock24(int h, int m, int s) : hours(h, si::hour), minutes(m, si::minute), seconds(s, si::second) {}

  // Each field takes any duration (90 min is 5400 s);
  // a wrap past 60 s hands its periods on as minutes.
  void add_seconds(const auto& d)
  {
    seconds.on_wrap([&](auto&, auto carry) { add_minutes(carry * si::minute); }) += d;
  }
  void add_minutes(const auto& d)
  {
    minutes.on_wrap([&](auto&, auto carry) { add_hours(carry * si::hour); }) += d;
  }
  void add_hours(const auto& d) { hours += d; }

  // A time of day from a duration: whole days wrap away on the store, then one divmod per field.
  using day_seconds = quantity<si::second, inside<{0, 86400, bounds::half_open}>>;
  static clock24 from(const auto& since_midnight)
  {
    const day_seconds s{since_midnight, wrap};
    const auto [h, rest] = divmod(s, just<1> * si::hour);
    const auto [m, sec]  = divmod(rest, just<1> * si::minute);
    clock24 c;
    c.hours   = h * si::hour;
    c.minutes = m * si::minute;
    c.seconds = sec;
    return c;
  }

  // …and back: h + min + s runs in seconds on [0, 86399], a day_seconds as it stands.
  [[nodiscard]] day_seconds seconds_since_midnight() const { return hours + minutes + seconds; }

  friend std::ostream& operator<<(std::ostream& os, const clock24& c)
  {
    auto pad = [&](auto n)
    {
      if (n < 10)
        os << '0';
      os << n;
    };
    pad(c.hours.numerical_value_in(si::hour));
    os << ':';
    pad(c.minutes.numerical_value_in(si::minute));
    os << ':';
    pad(c.seconds.numerical_value_in(si::second));
    return os;
  }
};

// A calendar: days 1–30 → months 1–12 → years, which take the final carry.
struct date
{
  using day_of_month  = quantity<si::day, inside<{1   , 30  }, wrap>>;
  using month_of_year = quantity<month  , inside<{1   , 12  }, wrap>>;
  using calendar_year = quantity<year   , inside<{1900, 2200}>>;

  day_of_month  day   {1   , si::day};
  month_of_year months{1   , month};
  calendar_year years {2000, year};
  error         Error {}; // a year past 2200 lands here

  void add_days(const auto& d)
  {
    day.on_wrap([&](auto&, auto carry) { add_months(carry * month); }) += d;
  }
  void add_months(const auto& d)
  {
    months.on_wrap([&](auto&, auto carry) { years.on_error(Error) += carry * year; }) += d;
  }

  friend std::ostream& operator<<(std::ostream& os, const date& d)
  {
    const auto m {d.months.numerical_value_in(month)};
    const auto dd{d.day.numerical_value_in(si::day)};
    return os << d.years.numerical_value_in(year) << '-' << (m < 10 ? "0" : "") << m
              << '-' << (dd < 10 ? "0" : "") << dd;
  }
};

int main()
{
  // 1. The clock: each delta is a quantity,
  // so the carry through sec → min → hour counts the next unit up.
  clock24 t(23, 59, 45);
  std::cout << "start:      " << t << "\n";
  t.add_seconds(just<20> * si::second);
  std::cout << "+20 s:      " << t << "\n";
  t.add_minutes(just<90> * si::minute);
  std::cout << "+90 min:    " << t << "\n";
  t.add_seconds(just<1> * si::hour + just<30> * si::minute + just<30> * si::second); // 5430 s
  std::cout << "+1 h 30 min 30 s: " << t << "\n";
  if (to_string(t.hours) != "3 h" || to_string(t.minutes) != "0 min" || to_string(t.seconds) != "35 s")
    return 1;

  // The same steps as one duration: the cascade agrees with it, and the clock converts both ways.
  {
    const auto end{just<23> * si::hour + just<59> * si::minute + just<45> * si::second
                   + just<20> * si::second + just<90> * si::minute + just<5430> * si::second}; // past midnight
    if (t.seconds_since_midnight() != end % (just<1> * si::day))
      return 1;
    const clock24 back{clock24::from(t.seconds_since_midnight())};
    if (back.hours != t.hours || back.minutes != t.minutes || back.seconds != t.seconds)
      return 1;
    std::cout << "from count: " << clock24::from(end) << " (" << end << ", whole days wrap away)\n";
  }

  // 2. The calendar.
  date d;
  d.day    = just<15> * si::day;
  d.months = just<3> * month;
  d.years  = just<2026> * year;
  std::cout << "\nstart:      " << d << "\n";
  d.add_days(just<20> * si::day);
  std::cout << "+20 d:      " << d << "\n";
  d.add_months(just<11> * month);
  std::cout << "+11 mo:     " << d << "\n";
  d.add_days(just<400> * si::day); // cascades through all three fields
  std::cout << "+400 d:     " << d << "\n";
  if (d.years != just<2028> * year || d.months != just<4> * month || d.day != just<15> * si::day)
    return 1;

  // Weeks and the day of the week by one divmod: a week's grid excludes 0, so it cannot fail.
  using day_number = quantity<si::day, inside<{0, 1'000'000}, round_toward_zero>>;
  const auto [weeks, dow] = divmod(day_number{14003, si::day}, just<1> * week);
  std::cout << "day 14003 is " << weeks << " weeks + " << dow << " (0 d = the reference day's weekday)\n";
  if (weeks != 2000 || dow != just<3> * si::day)
    return 1;

  // With 1970-01-01 (a Thursday, 4 days after a Sunday) as day 0: 2008-05-04, a Sunday.
  constexpr std::array<const char*, 7> names{
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  const auto our{(just<4> * si::day + dow) % (just<1> * week)};
  std::cout << "1970-01-01 + 14003 d is a " << names[our.numerical_value_in(si::day)] << "\n";
  if (our != just<0> * si::day)
    return 1;
  return 0;
}

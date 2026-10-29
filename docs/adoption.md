# Adopting safe numbers: the pain of knowing

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

This page is not about an API. It is about the people who would use one.

If you write C++ for a living, you were taught a set of number types (`int`,
`unsigned`, `double`) and you were told, without anyone saying it out loud,
that they were fine. They are not fine. `int` overflows silently or, worse, the
optimiser assumes it never does. `double` cannot hold 0.1. Division truncates,
narrowing is silent, and nothing in the type says what a number counts or how
far it may go. These are not defects of your skill. They are defects of the
defaults, and they have been with us for decades.

Learning this can hurt. It can feel like discovering a crack in the floor you
have been standing on for your whole career. If reading the
[paper](int-considered-harmful.pdf) or the [README](../README.md) gave you a
moment of unease, that is not a sign that you did something wrong. It is a sign
that you care whether your programs are right. This page is for that moment.

## Mooers' Law

In 1959, at a panel of the American Documentation Institute, the information
scientist Calvin N. Mooers asked a question that every library author meets
sooner or later:

> We are all aware that some retrieval systems, although technically rather
> poor, nevertheless receive intensive use, while other systems, sometimes
> technically very much better, receive very little customer use. Why is this?

Replace "retrieval systems" with "number types" and the question is ours. `int`
is technically rather poor and in intensive use. Checked, ranged, unit-aware
number libraries have existed for years and are used very little. Mooers'
answer was not about technology at all:

> It is my considered opinion, from long experience, that our customers will
> continue to be reluctant to use information systems-however well
> devised-so long as one feature of our present intellectual and engineering
> climate prevails. This feature-and its prevalence is all too commonplace in
> many companies, laboratories and agencies-is that for many people it is
> more painful and troublesome to have information than for them not to have
> it. For people in such a situation, we can expect that they will tend to
> avoid using a retrieval system which is efficient at putting information
> into their hands.

He stated it as a law:

> **Mooers' Law:** An information retrieval system will tend not to be used
> whenever it is more painful and troublesome for a customer to have
> information than for him not to have it.

A type that knows its range is a retrieval system for facts about your own
code. The first time you put one in, it hands you information: this sum can
overflow, this division can see a zero, this conversion can lose the value,
these two numbers are not in the same unit. Mooers describes exactly what that
information costs the person who receives it:

> Having information is painful and troublesome. We all have experienced this.
> If you have information, you must first read it, which is not always easy.
> You must then try to understand it. To do this, you may have to think about
> it. The information may require you to make decisions about it or other
> information. The decisions may require action in the way of a troublesome
> program of work, or trips or painful interviews. Understanding the
> information may show that your work was wrong, or that your boss was wrong,
> or may show that your work was needless. Having information, you must be
> careful not to lose it. If nothing else, information piles up on your
> desk-unread. It is a nuisance to have it come to you. It is uncomfortable to
> have to do anything about it. Finally, if you do try to use the information
> properly, you may be accused of puttering instead of working. Then in the
> end, the incorporation of the information into the work you do may often not
> be noticed or appreciated. Work saved is seldom recognized. Work done-even in
> duplication-is well paid and rewarded.

Every line of that has a counterpart in a code base. A compile error must be
read and understood. It asks for a decision: widen the range, clamp, reject the
input, or rethink the formula. The decision may mean a change in a module
someone else owns. It may show that a function written years ago, reviewed and
shipped, was never right. And the bug you prevented will never appear in any
report: nobody thanks you for the outage that did not happen, while the
colleague who fixes a production incident at 3 a.m. is a hero.

Mooers also saw why raising such findings takes nerve:

> One must be courageous or imprudent, or both, to point out from the
> literature that a current laboratory project which has had an expensive
> history and full backing of the management was futile from the outset.

Saying "our arithmetic has been wrong since the first release" in a planning
meeting is that kind of courage. If you have felt reluctant to say it, you are
not weak. You are reacting reasonably to an environment that punishes the
messenger. Mooers himself called his law "a pessimistic and even a cynical
conclusion". This page argues the opposite, because his law also says how the
pessimism ends. More on that below.

## What not knowing has cost

The pain of knowing is real. The cost of not knowing is measured in lives and
in money. Each of the failures below was an arithmetic fact that no type
recorded, and each was found only after the damage.

| Event | Arithmetic cause | Loss |
|---|---|---|
| Patriot battery, Dhahran, 25 February 1991 | Time counted in tenths of a second was converted with 1/10 held in a 24-bit register. 1/10 has no exact binary value: after 100 hours the clock was off by 0.3433 s, and the range gate looked 687 m away from the incoming Scud [1] | 28 American soldiers killed |
| Therac-25 radiation therapy machine, 1985–1987 | Among several faults, a one-byte flag was set by incrementing it. Every 256th pass it wrapped to 0, and a safety check was skipped [2] | Six massive radiation overdoses; patients died or were seriously injured |
| Vancouver Stock Exchange index, 1982–1983 | The index was recomputed after every trade and truncated to three decimals each time. After 22 months it read about 520 instead of 1098.892 [3] | A market index off by half for almost two years |
| Ariane 5 Flight 501, 4 June 1996 | A 64-bit floating-point value was converted to a 16-bit signed integer by code reused from Ariane 4, where the value had been provably smaller. The conversion overflowed 37 s after lift-off [4] | Rocket and four Cluster satellites destroyed, reported at more than US$370 million |
| Mars Climate Orbiter, 23 September 1999 | One team's software produced pound-force seconds, the other expected newton seconds. No value was out of range: what it *meant* was never written down [5] | The spacecraft was lost entering Mars orbit |
| Boeing 787, 2015 | A counter in the generator control units overflowed after 248 days of continuous power, and all AC power could be lost in flight [6] | Found before any accident. Airlines had to switch the aircraft off periodically until new software arrived |

None of these teams were careless. The Ariane 5 code was correct for the rocket
it was written for. The Patriot was designed to run for a few hours at a time
and was asked to run for days. In every case, the range, the step size or the
unit was known to someone and written down nowhere a compiler could check it.

This is the part that is hard to sit with, and the part that matters most: the
information that would have prevented these losses existed. It was more
painful to have it than not to have it, until it was too late.

## The weight of knowing

If you have read this far, you may be feeling some of the following. All of
them are normal.

- **"I have shipped `int` for twenty years."** So has everyone. The feeling
  that new knowledge invalidates your past work is called cognitive dissonance,
  and the easy way out of it is to dismiss the knowledge. The better way out is
  to notice that your past work was done with the tools you were given, and it
  was good work by the standards of those tools.
- **"If I flag this, someone will ask why we did not see it before."** Fear of
  blame keeps more bugs in production than ignorance does. A finding today is
  not a verdict on yesterday. It is a gift to tomorrow.
- **"We have invested too much to change now."** That is the sunk-cost
  feeling, and it is strongest exactly when change is most valuable. Changing
  one variable is not a rewrite.
- **"There will be hundreds of errors."** There may be. Each is something the
  program always did, now visible. You do not have to fix them all today.

Be kind to yourself and to your colleagues here. The platform failed you, not
the other way around. Integer overflow, silent narrowing and unitless numbers
were design decisions made under the constraints of the 1970s, and we have all
inherited them.

## Making knowing cheap

Mooers' Law cannot be argued away, but it can be turned around: if having the
information is cheap and not having it is expensive, people will want it. That
is a design goal of `inside`, and each of these choices exists to lower the
pain of knowing.

- **One variable at a time.** You do not adopt `inside` by rewriting a code
  base. You change one declaration, at the place where a number enters your
  program, and let the compiler show you where that number goes:

  ```cpp
  // Before: int volume = std::stoi(text);   // any int, and nobody checks
  using volume = inside<{0, 100}>;           // what was always true, now written down

  const auto v = parse<volume>(text);        // expected<volume, errc>: the boundary is where to know
  if (!v)
      return reject(text);
  const auto louder = *v + volume{10};       // inside<{0, 110}>: the type widened, nothing wrapped
  ```

  `parse` and `try_make` turn untrusted input into a value or an error, at the
  boundary, once ([conversions.md](conversions.md)).
- **Checked by default.** Every store is checked unless you write `unsafe`, so
  you are never silently opted out of the information
  ([policies.md](policies.md)). When two types can never fit, the compiler says
  so before the program runs.
- **Errors that explain themselves.** A rejected store names the clause that
  failed, and the values involved ([diagnostics.md](diagnostics.md)). Reading
  the information should not require decoding template noise.
- **No price on the fast path.** Where the range proves a result fits, the
  generated code is the same as the `int` it replaces
  ([performance.md](performance.md)). Knowing does not cost cycles.
- **Try it without asking anyone.** The [single header](single-header.md)
  works on Compiler Explorer. You can find out what your formula really does in
  five minutes, privately, before you raise it with anyone.
- **Bring what you know.** If you think in Q formats, [fixed-point.md](fixed-point.md)
  maps them to grids. If you think in physical quantities,
  [units.md](units.md) puts the unit in the type, so the Mars Climate Orbiter
  mistake is a compile error.

## Changing the rewards

Mooers did not end on despair. He ended on a condition:

> On the other hand, there are situations where the diligent finding and use
> of information is stressed and rewarded, and where failure to find or to use
> information is severely punished. In such places, we can expect retrieval
> systems to be actively used and we can expect pressure from the information
> users themselves for better systems. This turns out to be true in practice.

Aviation, medical devices and spaceflight learned this from the failures above.
Their engineers now ask for better tools themselves. Any team can create the
same climate without waiting for a disaster:

- **Review arithmetic findings without blame.** Ask "what did the type not
  know?", not "who wrote this?".
- **Count what was prevented.** When a range or unit check rejects a change,
  write it down. A log of incidents that did not happen makes saved work
  visible, which Mooers said it seldom is.
- **Celebrate the compile error.** The error that stopped a build is the
  cheapest bug report you will ever receive. Treat it as good news in stand-up.
- **Make it safe to say "this was always wrong."** Leads and reviewers set the
  tone. Thank the person who found it.

## A better future

Memory safety went through the same story. For decades, buffer overflows were
treated as a fact of life and a matter of individual care. Once the industry
could see the whole class of bugs, and once the tools made seeing it cheap, the
conversation changed from blame to engineering. Arithmetic can take the same
path. When a number's range, step and unit are part of its type, overflow
becomes unreachable, division by zero becomes a value, rounding becomes a
decision and a unit mismatch stops the build. The grave arithmetic bugs become
rare, not because people became perfect, but because the types stopped asking
them to be.

You do not have to carry the last fifty years of arithmetic on your shoulders.
Start with one number you care about, write down what you already know about
it, and let the compiler help. That is enough for today.

## Sources

The Mooers quotations are from C. N. Mooers, remarks at the Annual Meeting of
the American Documentation Institute, Lehigh University, 24 October 1959,
reprinted as "Mooers' Law, or why some retrieval systems are used and others
are not", *Bulletin of the American Society for Information Science*,
October/November 1996
([archived copy](https://web.archive.org/web/20110608152902/http://findarticles.com/p/articles/mi_qa3633/is_199610/ai_n8749122/)).

1. U.S. General Accounting Office, *Patriot Missile Defense: Software Problem
   Led to System Failure at Dhahran, Saudi Arabia*, GAO/IMTEC-92-26, February
   1992, Appendix II ([pdf](https://www.gao.gov/assets/imtec-92-26.pdf)).
2. N. G. Leveson and C. S. Turner, "An Investigation of the Therac-25
   Accidents", *IEEE Computer* 26(7):18–41, July 1993.
3. Reported in *The Wall Street Journal*, 8 November 1983, p. 37, and *The
   Toronto Star*, November 1983.
4. J. L. Lions et al., *ARIANE 5 Flight 501 Failure: Report by the Inquiry
   Board*, ESA/CNES, July 1996.
5. NASA, *Mars Climate Orbiter Mishap Investigation Board Phase I Report*,
   November 1999.
6. Federal Aviation Administration, Airworthiness Directive 2015-09-07, Boeing
   787 generator control units, May 2015.

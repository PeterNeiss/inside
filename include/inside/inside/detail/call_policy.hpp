// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef INSIDE_INSIDE_DETAIL_CALL_POLICY_HPP
#define INSIDE_INSIDE_DETAIL_CALL_POLICY_HPP

#include <inside/inside/detail/config.hpp>

#include <inside/inside/detail/error.hpp>         // error, raise, constexpr_error
#include <inside/inside/detail/introspection.hpp> // insidable, policy_of_impl
#include <inside/inside/policy_flag.hpp>

#include <expected>
#include <type_traits>

// The run-time policy of a call or store:
// detail::call_policy<F, E>, compile-time flags F plus an optional E holding an ins::error& (empty by EBO otherwise);
// make_policy<F>(); and with_flags<O>(pol), a call's policy under the flags O, keeping its error&.
// The proxies of b.with<F>(), b.on_error(ec) and on_*() are in inside/detail/policy_ref.hpp.
namespace ins
{
  // call_policy derives from E for EBO: the reporting form (E == empty_ref) is zero-sized,
  // call_policy(ec) carries an ins::error& (E == error_ref).
  // No virtuals; the error channel reports `error` directly, with no <system_error>.
  // Users spell it make_policy<F>() or make_policy<F>(ec), or pass a flag.
  namespace detail
  {
    struct empty_ref {};
    struct error_ref
    {
      constexpr error_ref(error& ec) : Code{ec} {}
      error& Code;
    };
    template <policy_flag F = none, typename E = empty_ref>
    struct call_policy : E
    {
      constexpr call_policy() = default;
      constexpr call_policy(error& ec)
        requires std::same_as<E, error_ref>
        : E(ec) {}

      static constexpr policy_flag Flags = F;

      static constexpr bool test(policy_flag flag) { return has_flag(F, flag); }

      static constexpr bool checks_range()
      {
        if consteval
        {
          return true;
        }
        return is_checked(F);
      }

      static constexpr bool checks_rounding()
      {
        if consteval
        {
          return true;
        }
        return is_checked(F) && not test(snap);
      }

      // A cheap report, with no message built.
      // With an error& it records the code (sticky: the first error is kept), also during constant evaluation,
      // so try_make, parse and tan<Out> return error values in constexpr code.
      // Without one, a failure is reached only in a constant expression
      // (a run-time store the grids cannot prove needs a channel, so it does not compile),
      // and the build fails with a text that names the code and the fix;
      // at run time it is a precondition (a cursor past its end) or a library bug, sent to the handler.
      constexpr void report(error code) const
      {
        if constexpr (std::is_same_v<E, error_ref>)
        {
          // Inline, so a caller's local error stays in a register:
          // an out-of-line record would take its address (a stack frame and protector in the caller).
          if (E::Code == error{})
            E::Code = code;
        }
        else
        {
          if consteval
          {
#if INSIDE_HAS_EXCEPTIONS
            raise(code, store_failure_message(code));
#else
            store_constexpr_error(code);
#endif
          }
          raise(code);
        }
      }
    };

    call_policy(error&) -> call_policy<none, error_ref>;
  } // namespace detail

  namespace detail
  {
    // Whether T is a call_policy: the free functions use it so P never binds an action tag.
    template <typename T>
    inline constexpr bool is_policy_v = false;
    template <policy_flag F, typename E>
    inline constexpr bool is_policy_v<call_policy<F, E>> = true;

    // The concept form, without cvref, so it matches a forwarded call_policy<F, E> reference.
    template <typename T>
    concept policy_like = is_policy_v<std::remove_cvref_t<T>>;

    // policy_of<call_policy<F, E>> is F,
    // so the value+policy constructor and policy_ref's conversion fold the call's flags into their
    // inside_assignable check: a one-shot clamp or rounding relaxes the constraint it enables.
    template <policy_flag F, typename E>
    inline constexpr policy_flag policy_of_impl<call_policy<F, E>> = F;

    // Whether a policy carries an ins::error&:
    // the free functions then report a failure into it as well as returning the std::unexpected.
    template <typename T>
    inline constexpr bool uses_error_ref = false;
    template <policy_flag F>
    inline constexpr bool uses_error_ref<call_policy<F, error_ref>> = true;
  } // namespace detail

  template <policy_flag F = detail::none>
  [[nodiscard]] constexpr auto make_policy()
  {
    return detail::call_policy<F, detail::empty_ref>{};
  }

  template <policy_flag F = detail::none>
  [[nodiscard]] constexpr auto make_policy(error& ec)
  {
    return detail::call_policy<F, detail::error_ref>{ec};
  }

  namespace detail
  {
    // The zero-divisor result of division and modulo: the error, also set in an error& policy.
    template <insidable Result, typename P>
    constexpr std::expected<Result, error> report_or_unexpected(P&& policy, error code)
    {
      if constexpr (uses_error_ref<std::remove_cvref_t<P>>)
        policy.report(code);
      return std::unexpected{code};
    }
  } // namespace detail

  namespace detail
  {
    // A call's one-shot policy (a named flag, or a call_policy from make_policy) as a call_policy with the flags O,
    // keeping its error& if it has one.
    template <policy_flag O, class Pol>
    constexpr auto with_flags(Pol&& pol)
    {
      if constexpr (uses_error_ref<std::remove_cvref_t<Pol>>)
        return call_policy<O, error_ref>{pol.Code};
      else
        return call_policy<O, empty_ref>{};
    }
  } // namespace detail
} // namespace ins

#endif // INSIDE_INSIDE_DETAIL_CALL_POLICY_HPP

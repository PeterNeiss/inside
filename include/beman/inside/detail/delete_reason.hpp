// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#ifndef BEMAN_INSIDE_DETAIL_DELETE_REASON_HPP
#define BEMAN_INSIDE_DETAIL_DELETE_REASON_HPP

//---------------------------------------------------------------------------
// A deleted overload with a reason: `= BEMAN_INSIDE_DELETE("…")`. It takes an
// operation's failing case, so `requires { a + b; }` is false and the error
// names the reason (= delete("…"), C++26, accepted as an extension in C++23
// by GCC 15+ and Clang 19+; older compilers get a plain `= delete`).
// Declarations using it sit between BEMAN_INSIDE_DELETE_BEGIN and _END,
// which silence the extension warning.
//---------------------------------------------------------------------------
#if defined(__cpp_deleted_function) || (defined(__clang__) && __clang_major__ >= 19) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 15)
    #define BEMAN_INSIDE_DELETE(reason) delete (reason)
    #define BEMAN_INSIDE_DELETE_BEGIN \
        _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wc++26-extensions\"")
    #define BEMAN_INSIDE_DELETE_END _Pragma("GCC diagnostic pop")
#else
    #define BEMAN_INSIDE_DELETE(reason) delete
    #define BEMAN_INSIDE_DELETE_BEGIN
    #define BEMAN_INSIDE_DELETE_END
#endif

#endif // BEMAN_INSIDE_DETAIL_DELETE_REASON_HPP

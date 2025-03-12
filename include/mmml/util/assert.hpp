#ifndef MMML_ASSERT_HPP
#define MMML_ASSERT_HPP

#include <source_location>
#include <string_view>

#include "mmml/fwd.hpp"

namespace mmml {

enum struct Assertion_Error_Type : Default_Underlying { expression, unreachable };

struct Assertion_Error {
    Assertion_Error_Type type;
    std::u8string_view message;
    std::source_location location;
};

#ifdef __EXCEPTIONS
#define MMML_RAISE_ASSERTION_ERROR(...) (throw __VA_ARGS__)
#else
#define MMML_RAISE_ASSERTION_ERROR(...) ::std::exit(3)
#endif

// Expects an expression.
// If this expression (after contextual conversion to `bool`) is `false`,
// throws an `Assertion_Error` of type `expression`.
#define MMML_ASSERT(...)                                                                           \
    ((__VA_ARGS__) ? void()                                                                        \
                   : MMML_RAISE_ASSERTION_ERROR(::mmml::Assertion_Error {                          \
                         ::mmml::Assertion_Error_Type::expression, u8## #__VA_ARGS__,              \
                         ::std::source_location::current() }))

/// Expects a string literal.
/// Unconditionally throws `Assertion_Error` of type `unreachable`.
#define MMML_ASSERT_UNREACHABLE(...)                                                               \
    MMML_RAISE_ASSERTION_ERROR(::mmml::Assertion_Error {                                           \
        ::mmml::Assertion_Error_Type::unreachable, ::std::u8string_view(__VA_ARGS__),              \
        ::std::source_location::current() })

#define MMML_IS_CONTEXTUALLY_BOOL_CONVERTIBLE(...) requires { (__VA_ARGS__) ? 1 : 0; }

#ifdef NDEBUG
#define MMML_DEBUG_ASSERT(...) static_assert(MMML_IS_CONTEXTUALLY_BOOL_CONVERTIBLE(__VA_ARGS__))
#define MMML_DEBUG_UNREACHABLE(...)                                                                \
    static_assert(MMML_IS_CONTEXTUALLY_BOOL_CONVERTIBLE(__VA_ARGS__))
#else
#define MMML_DEBUG_ASSERT(...) MMML_ASSERT(__VA_ARGS__)
#define MMML_DEBUG_ASSERT_UNREACHABLE(...) MMML_ASSERT_UNREACHABLE(__VA_ARGS__)
#endif

#if __cplusplus >= 202302L
#define MMML_CPP23 1
#endif

#if defined(MMML_CPP23) && __has_cpp_attribute(assume)
#define MMML_ASSUME(...) [[assume(__VA_ARGS__)]]
#elif defined(__clang__)
#define MMML_ASSUME(...) __builtin_assume(__VA_ARGS__)
#else
#define MMML_ASSUME(...)
#endif

} // namespace mmml

#endif

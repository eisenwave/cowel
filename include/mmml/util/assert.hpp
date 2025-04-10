#ifndef MMML_ASSERT_HPP
#define MMML_ASSERT_HPP

#include "ulight/impl/assert.hpp"

namespace mmml {

using ulight::Assertion_Error;
using ulight::Assertion_Error_Type;

#define MMML_ASSERT(...) ULIGHT_ASSERT(__VA_ARGS__)
#define MMML_DEBUG_ASSERT(...) ULIGHT_DEBUG_ASSERT(__VA_ARGS__)

#define MMML_ASSERT_UNREACHABLE(...) ULIGHT_ASSERT_UNREACHABLE(__VA_ARGS__)
#define MMML_DEBUG_ASSERT_UNREACHABLE(...) ULIGHT_DEBUG_ASSERT_UNREACHABLE(__VA_ARGS__)

} // namespace mmml

#endif

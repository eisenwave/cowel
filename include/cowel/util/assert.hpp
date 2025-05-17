#ifndef COWEL_ASSERT_HPP
#define COWEL_ASSERT_HPP

#include "ulight/impl/assert.hpp"

namespace cowel {

using ulight::Assertion_Error;
using ulight::Assertion_Error_Type;

#define COWEL_ASSERT(...) ULIGHT_ASSERT(__VA_ARGS__)
#define COWEL_DEBUG_ASSERT(...) ULIGHT_DEBUG_ASSERT(__VA_ARGS__)

#define COWEL_ASSERT_UNREACHABLE(...) ULIGHT_ASSERT_UNREACHABLE(__VA_ARGS__)
#define COWEL_DEBUG_ASSERT_UNREACHABLE(...) ULIGHT_DEBUG_ASSERT_UNREACHABLE(__VA_ARGS__)

} // namespace cowel

#endif

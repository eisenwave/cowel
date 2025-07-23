#ifndef COWEL_CONTENT_STATUS_HPP
#define COWEL_CONTENT_STATUS_HPP

#include <concepts>
#include <string_view>

#include "cowel/util/assert.hpp"

#include "cowel/cowel.h"
#include "cowel/fwd.hpp"

namespace cowel {

enum struct Processing_Status : Default_Underlying {
    ok = COWEL_PROCESSING_OK,
    brk = COWEL_PROCESSING_BREAK,
    error = COWEL_PROCESSING_ERROR,
    error_brk = COWEL_PROCESSING_ERROR_BREAK,
    fatal = COWEL_PROCESSING_FATAL,
};

[[nodiscard]]
constexpr std::u8string_view status_name(Processing_Status status)
{
    using enum Processing_Status;
    switch (status) {
        COWEL_ENUM_STRING_CASE8(ok);
        COWEL_ENUM_STRING_CASE8(brk);
        COWEL_ENUM_STRING_CASE8(error);
        COWEL_ENUM_STRING_CASE8(error_brk);
        COWEL_ENUM_STRING_CASE8(fatal);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid status.");
}

/// @brief Returns `true` iff `status` is a non-error status.
[[nodiscard]]
constexpr bool status_is_ok(Processing_Status status) noexcept
{
    return status < Processing_Status::error;
}

/// @brief Returns `true` iff `status` is an error status.
[[nodiscard]]
constexpr bool status_is_error(Processing_Status status) noexcept
{
    return status >= Processing_Status::error;
}

/// @brief Returns `true` iff `status` indicates that control flow should continue,
/// regardless whether the status is successful or an error.
[[nodiscard]]
constexpr bool status_is_continue(Processing_Status status) noexcept
{
    return status == Processing_Status::ok || status == Processing_Status::error;
}

/// @brief Retruns `true` iff `status` indicates that control flow should break,
/// regardless whether the status is successful or an error.
[[nodiscard]]
constexpr bool status_is_break(Processing_Status status) noexcept
{
    return status == Processing_Status::brk //
        || status == Processing_Status::error_brk //
        || status == Processing_Status::fatal;
}

[[nodiscard]]
constexpr Processing_Status
status_concat(Processing_Status first, Processing_Status second) noexcept
{
    return status_is_break(first)          ? first
        : first == Processing_Status::ok   ? second
        : second == Processing_Status::ok  ? Processing_Status::error
        : second == Processing_Status::brk ? Processing_Status::error_brk
                                           : second;
}

template <std::same_as<Processing_Status>... S>
[[nodiscard]]
constexpr Processing_Status status_concat(S... s) noexcept
{
    Processing_Status result = Processing_Status::ok;
    ((result = status_concat(result, s)), ...);
    return result;
}

} // namespace cowel

#endif

#include <get_code_point_name.hpp>

#include "cowel/util/code_point_names.hpp"

namespace cowel {
namespace {

using Result = Fixed_String8<get_code_point_name::max_length>;

[[nodiscard]]
Result from_char_buf(const char* const buf, const std::size_t len) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return { std::u8string_view(reinterpret_cast<const char8_t*>(buf), len) };
}

// Alias lookup functions may return multiple aliases joined by commas
// when a code point has more than one alias in the same category.
// This returns the length up to (but not including) the first comma.
[[nodiscard]]
std::size_t first_alias_len(const char* const buf, const std::size_t len) noexcept
{
    for (std::size_t i = 0; i != len; ++i) {
        if (buf[i] == ',') {
            return i;
        }
    }
    return len;
}

} // namespace

Result code_point_name(const char32_t code_point) noexcept
{
    char buf[get_code_point_name::max_length];
    const std::size_t len = get_code_point_name::get_code_point_name(code_point, buf);
    return from_char_buf(buf, len);
}

Result code_point_display_name(const char32_t code_point) noexcept
{
    using namespace get_code_point_name;
    char buf[max_length];
    if (const std::size_t n
        = get_code_point_name::get_code_point_correction_alias(code_point, buf)) {
        return from_char_buf(buf, first_alias_len(buf, n));
    }
    if (const std::size_t n = get_code_point_name::get_code_point_name(code_point, buf)) {
        return from_char_buf(buf, n);
    }
    if (const std::size_t n = get_code_point_name::get_code_point_control_alias(code_point, buf)) {
        return from_char_buf(buf, first_alias_len(buf, n));
    }
    if (const std::size_t n
        = get_code_point_name::get_code_point_alternate_alias(code_point, buf)) {
        return from_char_buf(buf, first_alias_len(buf, n));
    }
    if (const std::size_t n = get_code_point_name::get_code_point_figment_alias(code_point, buf)) {
        return from_char_buf(buf, first_alias_len(buf, n));
    }
    return {};
}

} // namespace cowel

#ifndef COWEL_BIG_INT_OPS_HPP
#define COWEL_BIG_INT_OPS_HPP

#include <string>

#include "cowel/big_int.hpp"

namespace cowel {

[[nodiscard]]
inline std::string to_string(const Big_Int& x, const int base = 10, const bool to_upper = false)
{
    std::string result;
    x.print_to([&](const std::string_view str) { result += str; }, base, to_upper);
    return result;
}

[[nodiscard]]
inline std::u8string to_u8string(const Big_Int& x, const int base = 10, const bool to_upper = false)
{
    std::u8string result;
    x.print_to([&](const std::u8string_view str) { result += str; }, base, to_upper);
    return result;
}

} // namespace cowel

#endif

#ifndef COWEL_COMPILATION_STAGE_HPP
#define COWEL_COMPILATION_STAGE_HPP

#include <compare>

#include "cowel/fwd.hpp"

namespace cowel {

enum struct Compilation_Stage : Default_Underlying { //
    load_file,
    parse,
    process
};

[[nodiscard]]
constexpr std::strong_ordering operator<=>(Compilation_Stage a, Compilation_Stage b)
{
    return static_cast<Default_Underlying>(a) <=> static_cast<Default_Underlying>(b);
}

} // namespace cowel

#endif

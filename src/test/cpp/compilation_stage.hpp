#ifndef MMML_COMPILATION_STAGE_HPP
#define MMML_COMPILATION_STAGE_HPP

#include "mmml/fwd.hpp"

namespace mmml {

enum struct Compilation_Stage : Default_Underlying { //
    load_file,
    parse,
    process
};

[[nodiscard]]
constexpr auto operator<=>(Compilation_Stage a, Compilation_Stage b)
{
    return static_cast<Default_Underlying>(a) <=> static_cast<Default_Underlying>(b);
}

} // namespace mmml

#endif

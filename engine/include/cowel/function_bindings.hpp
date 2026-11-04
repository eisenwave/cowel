#ifndef COWEL_FUNCTION_BINDINGS_HPP
#define COWEL_FUNCTION_BINDINGS_HPP

#include <memory_resource>
#include <utility>
#include <vector>

#include "cowel/value.hpp"

namespace cowel {

/// @brief Maps parameter names to the values that they were matched with.
using Function_Binding_Map = std::pmr::vector<std::pair<std::pmr::u8string, Value>>;

/// @brief A chain of function parameter bindings.
/// Each node in the chain corresponds to one level of function call nesting.
/// `parent` points to the bindings of the outer (caller) function,
/// or `nullptr` if there is no outer function.
struct Function_Binding_Chain {
    Function_Binding_Map bindings;
    const Function_Binding_Chain* parent = nullptr;
};

} // namespace cowel

#endif

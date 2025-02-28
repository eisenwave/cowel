#ifndef MMML_DIAGNOSTIC_POLICY_HPP
#define MMML_DIAGNOSTIC_POLICY_HPP

#include <string_view>

#include "mmml/fwd.hpp"
#include "mmml/io_error.hpp"

#include "compilation_stage.hpp"

namespace mmml {

enum struct Policy_Action {
    /// @brief Immediate success.
    success,
    /// @brief Immediate failure.
    failure,
    /// @brief Keep going.
    keep_going
};

[[nodiscard]]
constexpr bool is_exit(Policy_Action action)
{
    return action != Policy_Action::keep_going;
}

struct Diagnostic_Policy {
    virtual bool is_success() const = 0;

    virtual Policy_Action error(IO_Error_Code) = 0;

    virtual Policy_Action done(Compilation_Stage) = 0;
};

} // namespace mmml

#endif

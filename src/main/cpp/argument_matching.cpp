#include <cstddef>
#include <span>
#include <string_view>

#include "mmml/util/assert.hpp"

#include "mmml/ast.hpp"
#include "mmml/directive_arguments.hpp"

namespace mmml {

void match_parameters_and_arguments(
    std::span<int> out_indices,
    std::span<Argument_Status> out_status,
    std::span<const std::u8string_view> parameters,
    std::span<const ast::Argument> arguments,
    std::u8string_view source
)
{
    MMML_ASSERT(out_indices.size() == parameters.size());
    MMML_ASSERT(out_status.size() == arguments.size());

    // Setup.
    for (int& i : out_indices) {
        i = -1;
    }
    for (Argument_Status& s : out_status) {
        s = Argument_Status::unmatched;
    }

    // Named argument matching.
    for (std::size_t arg_index = 0; arg_index < arguments.size(); ++arg_index) {
        if (!arguments[arg_index].has_name()) {
            continue;
        }
        const std::u8string_view arg_name = arguments[arg_index].get_name(source);
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (arg_name == parameters[i]) {
                if (out_indices[i] == -1) {
                    out_indices[i] = int(arg_index);
                    out_status[arg_index] = Argument_Status::ok;
                }
                else {
                    out_status[arg_index] = Argument_Status::duplicate_named;
                }
                break;
            }
        }
    }

    // Positional argument matching.
    for (std::size_t arg_index = 0; arg_index < arguments.size(); ++arg_index) {
        if (arguments[arg_index].has_name()) {
            continue;
        }
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (out_indices[i] == -1) {
                out_indices[i] = int(arg_index);
                out_status[arg_index] = Argument_Status::ok;
            }
            break;
        }
    }
}

} // namespace mmml

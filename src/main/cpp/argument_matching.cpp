#include <cstddef>
#include <span>
#include <string_view>

#include "cowel/util/assert.hpp"

#include "cowel/ast.hpp"
#include "cowel/directive_arguments.hpp"

namespace cowel {

void match_parameters_and_arguments(
    std::span<int> out_indices,
    std::span<Argument_Status> out_status,
    std::span<const std::u8string_view> parameters,
    std::span<const ast::Argument> arguments,
    Parameter_Match_Mode mode
)
{
    COWEL_ASSERT(out_indices.size() == parameters.size());
    COWEL_ASSERT(out_status.size() == arguments.size());

    for (int& i : out_indices) {
        i = -1;
    }
    for (Argument_Status& s : out_status) {
        s = Argument_Status::unmatched;
    }

    if (mode != Parameter_Match_Mode::only_positional) {
        for (std::size_t arg_index = 0; arg_index < arguments.size(); ++arg_index) {
            if (!arguments[arg_index].has_name()) {
                continue;
            }
            const std::u8string_view arg_name = arguments[arg_index].get_name();
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
    }

    if (mode != Parameter_Match_Mode::only_named) {
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
}

} // namespace cowel

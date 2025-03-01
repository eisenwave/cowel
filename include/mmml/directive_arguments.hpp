#ifndef MMML_DIRECTIVE_ARGUMENTS_HPP
#define MMML_DIRECTIVE_ARGUMENTS_HPP

#include <span>

#include "mmml/ast.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

enum struct Argument_Status : Default_Underlying {
    /// @brief The argument was matched successfully.
    ok,
    /// @brief No corresponding parameter could be found for the argument.
    unmatched,
    /// @brief The argument is named,
    /// and more than one argument for the same parameter was provided.
    duplicate_named,
};

/// @brief Matches a list of parameters to a list of arguments for some directive.
///
/// First, any named arguments are matched to parameters with that name.
/// Then, any remaining positional arguments are matched in increasing order to
/// remaining parameters.
/// @param out_indices for each parameter, stores the index of the matched argument, or `-1`
/// if none could be matched
/// @param out_status for each argument, the resulting status after matching
/// @param parameters a span of parameter names
/// @param arguments a span of arguments, which could be named or unnamed
/// @param source the source
void match_parameters_and_arguments(
    std::span<int> out_indices,
    std::span<Argument_Status> out_status,
    std::span<const std::string_view> parameters,
    std::span<const ast::Argument> arguments,
    std::string_view source
);

/// @brief Makes parameter/argument matching convenient for a fixed sequence of arguments.
struct [[nodiscard]] Argument_Matcher {
private:
    std::pmr::vector<Argument_Status> m_statuses;
    std::pmr::vector<int> m_indices;
    std::span<const std::string_view> m_parameters;

public:
    Argument_Matcher(
        std::span<const std::string_view> parameters,
        std::pmr::memory_resource* memory
    )
        : m_statuses { memory }
        , m_indices { parameters.size(), memory }
        , m_parameters { parameters }
    {
    }

    /// @brief Matches a sequence of arguments using `match_parameters_and_arguments`.
    /// Other member function can subsequently access the results.
    void match(std::span<const ast::Argument> arguments, std::string_view source)
    {
        m_statuses.resize(arguments.size());
        match_parameters_and_arguments(m_indices, m_statuses, m_parameters, arguments, source);
    }

    /// @brief Returns the matched argument index for the parameter with the given name,
    /// or `-1` if no argument matches.
    /// The parameter name shall be one of the `parameters` passed into the constructor.
    [[nodiscard]]
    int get_argument_index(std::string_view parameter_name) const
    {
        for (std::size_t i = 0; i < m_parameters.size(); ++i) {
            if (m_parameters[i] == parameter_name) {
                return m_indices[i];
            }
        }
        MMML_ASSERT_UNREACHABLE("Invalid parameter name");
    }

    /// @brief Returns the indices of the argument for each parameter,
    /// i.e. `parameter_indices()[i]` stores which argument the parameter `i` matches.
    /// If no argument matches the parameter, `-1` is stored instead.
    [[nodiscard]]
    std::span<const int> parameter_indices() const
    {
        return m_indices;
    }

    /// @brief Returns the argument statuses.
    /// Shall only be used after calling `match`.
    [[nodiscard]]
    std::span<const Argument_Status> argument_statuses() const
    {
        return m_statuses;
    }
};

} // namespace mmml

#endif

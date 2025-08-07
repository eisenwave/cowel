#ifndef COWEL_DIRECTIVE_BEHAVIOR_HPP
#define COWEL_DIRECTIVE_BEHAVIOR_HPP

#include <cstddef>
#include <span>
#include <string_view>

#include "cowel/ast.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/fwd.hpp"

namespace cowel {

struct Invocation {
    /// @brief The name which names the invoked directive.
    /// In the case of e.g. `\x`, this is simply `x`,
    /// but in the case of `\cowel_invoke[x]`, it is `x`.
    std::u8string_view name;
    /// @brief The directive responsible for the invocation.
    /// This may not necessarily be a directive matching the behavior,
    /// but a directive like `\cowel_invoke` which performs that invocation programmatically.
    const ast::Directive& directive;
    /// @brief The arguments with which the directive is invoked.
    /// Note that this is not necessarily the same as `directive->get_arguments()`.
    std::span<const ast::Argument> arguments;
    /// @brief The content with which the directive is invoked.
    /// Note that this is not necessarily the same as `directive->get_content()`.
    std::span<const ast::Content> content;
    /// @brief The stack frame index of the invocation.
    /// This is always at least `1`
    /// because `0` indicates the document top level,
    /// with each level of invocation being one greater than the level below.
    std::size_t frame_index;
};

[[nodiscard]]
inline Invocation make_invocation(const ast::Directive& d, std::size_t frame_index)
{
    return {
        .name = d.get_name(),
        .directive = d,
        .arguments = d.get_arguments(),
        .content = d.get_content(),
        .frame_index = frame_index,
    };
}

/// @brief Implements behavior that one or multiple directives should have.
struct Directive_Behavior {
    constexpr Directive_Behavior() = default;

    [[nodiscard]]
    virtual Processing_Status operator()(Content_Policy& out, const Invocation&, Context&) const
        = 0;
};

} // namespace cowel

#endif

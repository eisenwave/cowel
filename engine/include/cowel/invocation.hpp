#ifndef COWEL_INVOCATION_HPP
#define COWEL_INVOCATION_HPP

#include <span>
#include <string_view>

#include "cowel/fwd.hpp"

#include "cowel/syntax/ast.hpp"

namespace cowel {

struct Invocation {
    /// @brief The name which names the invoked directive.
    /// In the case of e.g. `\x`, this is simply `x`,
    /// but in the case of `\cowel_invoke(x)`, it is `x`.
    std::u8string_view name;
    /// @brief The directive responsible for the invocation.
    /// This may not necessarily be a directive matching the behavior,
    /// but a directive like `\cowel_invoke` which performs that invocation programmatically.
    const ast::Directive& directive;
    /// @brief The arguments with which the directive is invoked.
    const ast::Primary* arguments;
    /// @brief The block arguments with which the directive is invoked.
    std::span<const ast::Primary> block_arguments;
    /// @brief The stack frame in which the block arguments appear.
    /// A null reference means root content, i.e. the document top level.
    /// All content in a macro definition (and arguments of directives within)
    /// have the same frame as that invocation.
    /// Intuitively, all visible content inside a macro lives in the same frame,
    /// just like in a C++ function.
    Frame_Ref content_frame;
    /// @brief The stack frame which is processing this invocation.
    /// This is the frame which was pushed for this invocation when the invocation
    /// comes from the AST,
    /// and the content frame when the invocation was synthesized
    /// (e.g. for the pseudo-directives of `\math`).
    ///
    /// Unlike `content_frame`,
    /// this reference is only ever set for the invocation which is currently in flight;
    /// the copy of the invocation which is stored in its own `Stack_Frame` leaves it null.
    Frame_Ref call_frame;

    [[nodiscard]]
    bool has_arguments() const
    {
        return arguments && arguments->has_members();
    }

    [[nodiscard]]
    std::span<const ast::Group_Member> get_arguments_span() const
    {
        if (arguments) {
            return arguments->get_members();
        }
        return {};
    }

    [[nodiscard]]
    File_Source_Span get_arguments_source_span() const
    {
        return arguments ? arguments->get_source_span() : directive.get_name_span();
    }

    [[nodiscard]]
    bool has_block_arguments() const
    {
        return !block_arguments.empty();
    }

    [[nodiscard]]
    std::span<const ast::Primary> get_block_arguments() const
    {
        return block_arguments;
    }

    [[nodiscard]]
    File_Source_Span get_block_arguments_source_span() const
    {
        if (block_arguments.empty()) {
            return directive.get_source_span();
        }
        const File_Source_Span first = block_arguments.front().get_source_span();
        const File_Source_Span last = block_arguments.back().get_source_span();
        return first.with_length(last.end() - first.begin);
    }
};

/// @brief Creates a new `Invocation` object from a directive,
/// which is what we consider a "direct call".
/// @param d The directive which is invoked.
/// @param content_frame The frame in which the directive appears.
/// @param call_frame The frame of the invocation.
/// @param args Arguments for this invocation.
[[nodiscard]]
inline Invocation make_invocation( //
    const ast::Directive& d,
    Frame_Ref content_frame,
    Frame_Ref call_frame
)
{
    return {
        .name = d.get_name(),
        .directive = d,
        .arguments = d.get_arguments(),
        .block_arguments = d.get_block_arguments(),
        .content_frame = std::move(content_frame),
        .call_frame = std::move(call_frame),
    };
}

} // namespace cowel

#endif

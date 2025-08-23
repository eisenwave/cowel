#ifndef COWEL_INVOCATION_HPP
#define COWEL_INVOCATION_HPP

#include <span>
#include <string_view>

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"

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
    const ast::Group* arguments;
    /// @brief The content with which the directive is invoked.
    const ast::Content_Sequence* content;
    /// @brief The stack frame index of the content.
    /// For root content, this is zero.
    /// All content in a macro definition (and arguments of directives within)
    /// have the same frame index as that invocation.
    /// Intuitively, all visible content inside a macro has the same frame index,
    /// just like in a C++ function.
    Frame_Index content_frame;
    /// @brief The stack frame index of the invocation.
    /// This is always at least `1`
    /// because `0` indicates the document top level,
    /// with each level of invocation being one greater than the level below.
    Frame_Index call_frame;

    [[nodiscard]]
    bool has_arguments() const
    {
        return arguments && !arguments->empty();
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
    bool has_empty_content() const
    {
        return !content || content->empty();
    }

    [[nodiscard]]
    std::span<const ast::Content> get_content_span() const
    {
        if (content) {
            return content->get_elements();
        }
        return {};
    }

    [[nodiscard]]
    File_Source_Span get_content_source_span() const
    {
        return content ? content->get_source_span() : directive.get_source_span();
    }
};

/// @brief Creates a new `Invocation` object from a directive,
/// which is what we consider a "direct call".
/// @param d The directive which is invoked.
/// @param content_frame The frame index of the directive.
/// @param call_frame The frame index of the invocation.
/// @param args Arguments for this invocation.
[[nodiscard]]
inline Invocation make_invocation( //
    const ast::Directive& d,
    Frame_Index content_frame,
    Frame_Index call_frame
)
{
    return {
        .name = d.get_name(),
        .directive = d,
        .arguments = d.get_arguments(),
        .content = d.get_content(),
        .content_frame = content_frame,
        .call_frame = call_frame,
    };
}

} // namespace cowel

#endif

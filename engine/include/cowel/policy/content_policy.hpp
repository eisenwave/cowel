#ifndef COWEL_CONTENT_POLICY_HPP
#define COWEL_CONTENT_POLICY_HPP

#include <variant>

#include "cowel/util/char_sequence.hpp"

#include "cowel/content_status.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

#include "cowel/syntax/ast.hpp"

namespace cowel {

enum struct Text_Sink_Flags : Default_Underlying {
    /// @brief No flags are set.
    none = 0,
    /// @brief `true` if the sink discards input with `Output_Language::text`.
    discard_text = 1 << 0,
    /// @brief `true` if the sink discards input with `Output_Language::html`.
    discard_html = 1 << 1,
    /// @brief `true` if the sink discards all input.
    discard = discard_text | discard_html,
};

[[nodiscard]]
constexpr Text_Sink_Flags operator|(const Text_Sink_Flags x, const Text_Sink_Flags y)
{
    return Text_Sink_Flags(Default_Underlying(x) | Default_Underlying(y));
}

[[nodiscard]]
constexpr Text_Sink_Flags operator&(const Text_Sink_Flags x, const Text_Sink_Flags y)
{
    return Text_Sink_Flags(Default_Underlying(x) & Default_Underlying(y));
}

struct Text_Sink {
private:
    Text_Sink_Flags m_flags;

public:
    [[nodiscard]]
    constexpr explicit Text_Sink(const Text_Sink_Flags flags) noexcept
        : m_flags { flags }
    {
    }

    [[nodiscard]]
    constexpr Text_Sink_Flags get_flags() const noexcept
    {
        return m_flags;
    }

    [[nodiscard]]
    constexpr bool has_flags(const Text_Sink_Flags flags) const noexcept
    {
        return (m_flags & flags) != Text_Sink_Flags::none;
    }

    /// @brief Attempts to write `str` in the specified `language`.
    /// @returns `true` iff the language was accepted.
    /// `get_language()` is always accepted.
    virtual void write(Char_Sequence8 str, Output_Language language) = 0;
};

/// @brief A content policy can receive different kinds of content as well as text,
/// and controls how these are processed.
///
/// A content policy has a single target language which it expects its given content to be in.
/// If given content in different languages, it can choose to ignore it,
/// transform it into its expected format, etc.
///
/// Furthermore, when a content policy receives different kinds of AST content,
/// it decides how those should be processed.
/// It can even choose to turn comments into text, ignore directives entirely, etc.
struct Content_Policy : virtual Text_Sink {

    [[nodiscard]]
    explicit Content_Policy(const Text_Sink_Flags flags = Text_Sink_Flags::none) noexcept
        : Text_Sink { flags }
    {
    }

    [[nodiscard]]
    virtual Processing_Status //
    consume(const ast::Primary& text, Frame_Index frame, Context& context)
        = 0;
    [[nodiscard]]
    virtual Processing_Status //
    consume(const ast::Directive& directive, Frame_Index frame, Context& context)
        = 0;

    [[nodiscard]]
    virtual Processing_Status //
    consume(const ast::Expression& expression, Frame_Index frame, Context& context)
        = 0;

    [[nodiscard]]
    Processing_Status //
    consume_content(const ast::Markup_Element& content, Frame_Index frame, Context& context)
    {
        return std::visit([&](const auto& c) { return consume(c, frame, context); }, content);
    }
};

} // namespace cowel

#endif

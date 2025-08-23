#ifndef COWEL_DIRECTIVE_PROCESSING_HPP
#define COWEL_DIRECTIVE_PROCESSING_HPP

#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/severity.hpp"

#include "cowel/ast.hpp"
#include "cowel/content_status.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

namespace cowel {

/// @brief Returns a string view to containing code units that `escape` corresponds to.
/// For most escape sequences, this returns `escape`.
/// For LF and CRLF escapes, this is an empty string view.
/// @param escape The escaped character(s), not including the initial `\`.
[[nodiscard]]
std::u8string_view expand_escape(std::u8string_view escape);

/// @brief Returns a string view to static storage corresponding to the code units
/// that `escape` corresponds to.
/// For most escape sequences, this is simply the character following the initial `\`.
/// For LF and CRLF escapes, this is an empty string.
[[nodiscard]]
inline std::u8string_view expand_escape(const ast::Escaped& escape)
{
    return expand_escape(escape.get_escaped());
}

[[nodiscard]]
std::span<const ast::Content> trim_blank_text_left(std::span<const ast::Content>);
[[nodiscard]]
std::span<const ast::Content> trim_blank_text_right(std::span<const ast::Content>);

/// @brief Trims leading and trailing completely blank text content.
[[nodiscard]]
std::span<const ast::Content> trim_blank_text(std::span<const ast::Content>);

enum struct To_Plaintext_Mode : Default_Underlying { //
    normal,
    no_side_effects,
    trimmed,
};

enum struct To_Plaintext_Status : Default_Underlying { //
    ok,
    some_ignored,
    error
};

[[nodiscard]]
Processing_Status invoke(
    Content_Policy& out,
    const ast::Directive& directive,
    std::u8string_view name,
    const ast::Group* args,
    const ast::Content_Sequence* content,
    Frame_Index content_frame,
    Context& context
);

/// @brief Convenience function which performs a direct call of a directive
/// via `invoke`.
[[nodiscard]]
Processing_Status invoke_directive(
    Content_Policy& out,
    const ast::Directive& d,
    Frame_Index content_frame,
    Context& context
);

[[nodiscard]]
const ast::Content_Sequence* as_content_or_error(
    const ast::Value& value,
    Context& context,
    Severity error_severity = Severity::error
);

[[nodiscard]]
inline const ast::Content_Sequence*
as_content_or_fatal_error(const ast::Value& value, Context& context)
{
    return as_content_or_error(value, context, Severity::fatal);
}

template <std::ranges::input_range R, typename Consumer>
    requires std::is_invocable_r_v<Processing_Status, Consumer, std::ranges::range_reference_t<R>>
[[nodiscard]]
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
Processing_Status process_greedy(R&& content, Consumer consumer)
{
    bool error = false;
    auto end = std::ranges::end(content);
    for (auto it = std::ranges::begin(content); it != end; ++it) {
        const auto status = consumer(*it);
        switch (status) {
        case Processing_Status::ok: continue;
        case Processing_Status::brk:
            return error ? Processing_Status::error_brk : Processing_Status::brk;
        case Processing_Status::error: error = true; continue;
        case Processing_Status::error_brk:
        case Processing_Status::fatal: return status;
        }
    }
    return error ? Processing_Status::error : Processing_Status::ok;
}

template <std::ranges::input_range R, typename Consumer>
    requires std::is_invocable_r_v<Processing_Status, Consumer, std::ranges::range_reference_t<R>>
[[nodiscard]]
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
Processing_Status process_lazy(R&& content, Consumer consumer)
{
    for (auto& c : content) {
        const auto status = consumer(c);
        if (status != Processing_Status::ok) {
            return status;
        }
    }
    return Processing_Status::ok;
}

[[nodiscard]]
inline Processing_Status consume_all(
    Content_Policy& out,
    std::span<const ast::Content> content,
    Frame_Index frame,
    Context& context
)
{
    return process_greedy(content, [&](const ast::Content& c) {
        return out.consume_content(c, frame, context);
    });
}

[[nodiscard]]
inline Processing_Status consume_all(
    Content_Policy& out,
    const ast::Value& value,
    Frame_Index frame,
    Context& context,
    Processing_Status error_status = Processing_Status::error
)
{
    const auto* const content = as_content_or_error(value, context);
    if (!content) {
        return error_status;
    }
    return process_greedy(content->get_elements(), [&](const ast::Content& c) {
        return out.consume_content(c, frame, context);
    });
}

[[nodiscard]]
Processing_Status consume_all_trimmed(
    Content_Policy& out,
    std::span<const ast::Content> content,
    Frame_Index,
    Context& context
);

[[nodiscard]]
Processing_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Frame_Index frame,
    Context& context
);

[[nodiscard]]
Processing_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Value& value,
    Frame_Index frame,
    Context& context
);

struct Plaintext_Result {
    Processing_Status status;
    std::u8string_view string;
};

[[nodiscard]]
Plaintext_Result to_plaintext_optimistic(
    std::pmr::vector<char8_t>& buffer,
    std::span<const ast::Content> content,
    Frame_Index frame,
    Context& context
);

enum struct To_HTML_Mode : Default_Underlying {
    direct,
    paragraphs,
    trimmed,
    paragraphs_trimmed,
};

[[nodiscard]]
constexpr bool to_html_mode_is_trimmed(To_HTML_Mode mode)
{
    return mode == To_HTML_Mode::trimmed || mode == To_HTML_Mode::paragraphs_trimmed;
}

[[nodiscard]]
constexpr bool to_html_mode_is_paragraphed(To_HTML_Mode mode)
{
    return mode == To_HTML_Mode::paragraphs || mode == To_HTML_Mode::paragraphs_trimmed;
}

enum struct Paragraphs_State : bool {
    outside,
    inside,
};

[[nodiscard]]
Processing_Status match_empty_arguments(
    const Invocation& call,
    Context& context,
    Processing_Status fail_status = Processing_Status::error
);

void diagnose(
    Syntax_Highlight_Error error,
    std::u8string_view lang,
    const Invocation& call,
    Context& context
);

[[nodiscard]]
Processing_Status named_arguments_to_attributes(
    Text_Buffer_Attribute_Writer& out,
    std::span<const ast::Group_Member> arguments,
    Frame_Index frame,
    Context& context,
    Attribute_Style style = Attribute_Style::double_if_needed
);

[[nodiscard]]
Processing_Status named_argument_to_attribute(
    Text_Buffer_Attribute_Writer& out,
    const ast::Group_Member& a,
    Frame_Index frame,
    Context& context,
    Attribute_Style style = Attribute_Style::double_if_needed
);

/// @brief Similar to `Result`,
/// but does not behave like a union,
/// but rather always contains a value.
template <typename T>
struct Greedy_Result {
private:
    T m_value;
    Processing_Status m_status = Processing_Status::ok;

public:
    [[nodiscard]]
    Greedy_Result(const T& value, Processing_Status status = Processing_Status::ok)
        : m_value { value }
        , m_status { status }
    {
    }

    [[nodiscard]]
    Greedy_Result(T&& value, Processing_Status status = Processing_Status::ok)
        : m_value { std::move(value) }
        , m_status { status }
    {
    }

    [[nodiscard]]
    constexpr explicit operator bool() const
    {
        return m_status == Processing_Status::ok;
    }

    [[nodiscard]]
    constexpr T& operator*()
    {
        COWEL_ASSERT(m_status == Processing_Status::ok);
        return m_value;
    }
    [[nodiscard]]
    constexpr const T& operator*() const
    {
        COWEL_ASSERT(m_status == Processing_Status::ok);
        return m_value;
    }

    [[nodiscard]]
    constexpr T* operator->()
    {
        COWEL_ASSERT(m_status == Processing_Status::ok);
        return std::addressof(m_value);
    }
    [[nodiscard]]
    constexpr const T* operator->() const
    {
        COWEL_ASSERT(m_status == Processing_Status::ok);
        return std::addressof(m_value);
    }

    [[nodiscard]]
    constexpr Processing_Status status() const
    {
        return m_status;
    }
};

/// @brief Uses the error behavior provided by `context` to process `call`.
/// Returns `on_success` if that generation succeeded.
[[nodiscard]]
Processing_Status try_generate_error(
    Content_Policy& out,
    const Invocation& call,
    Context& context,
    Processing_Status on_success = Processing_Status::error
);

/// @brief If `out` is a `Paragraph_Split_Content_Policy`
/// calls `out.activate_paragraphs_in_directive()`.
void try_inherit_paragraph(Content_Policy& out);

/// @brief If `out` is a `Paragraph_Split_Content_Policy`
/// calls `out.enter_paragraph()`.
void try_enter_paragraph(Content_Policy& out);

/// @brief If `out` is a `Paragraph_Split_Content_Policy`
/// calls `out.leave_paragraph()`.
void try_leave_paragraph(Content_Policy& out);

/// @brief If `display` is `in_line`, calls `try_enter_paragraph(out)`.
/// If `display` is `block`, calls `try_leave_paragraph(out)`.
/// Otherwise, has no effect.
void ensure_paragraph_matches_display(Content_Policy& out, Directive_Display display);

} // namespace cowel

#endif

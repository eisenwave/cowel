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
#include "cowel/util/result.hpp"

#include "cowel/content_status.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"
#include "cowel/value.hpp"

#include "cowel/syntax/ast.hpp"

namespace cowel {

[[nodiscard]]
std::span<const ast::Markup_Element> trim_blank_text_left(std::span<const ast::Markup_Element>);
[[nodiscard]]
std::span<const ast::Markup_Element> trim_blank_text_right(std::span<const ast::Markup_Element>);

/// @brief Trims leading and trailing completely blank text content.
[[nodiscard]]
std::span<const ast::Markup_Element> trim_blank_text(std::span<const ast::Markup_Element>);

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
Processing_Status splice_invocation(
    Content_Policy& out,
    const ast::Directive& directive,
    std::u8string_view name,
    const ast::Primary* args,
    const ast::Primary* content,
    Frame_Index content_frame,
    Context& context
);

/// @brief Convenience function which performs a direct call of a directive
/// via `invoke`.
[[nodiscard]]
Processing_Status splice_directive_invocation(
    Content_Policy& out,
    const ast::Directive& d,
    Frame_Index content_frame,
    Context& context
);

/// @brief If `context.collects_hovers()` and `node` has a real code point
/// (i.e. not a whitespace escape),
/// pushes a hover article of the form "U+XXXX" for the escape node.
void push_escape_hover(const ast::Primary& node, std::u8string_view code_units, Context& context);

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
inline Processing_Status splice_all(
    Content_Policy& out,
    std::span<const ast::Markup_Element> content,
    Frame_Index frame,
    Context& context
)
{
    return process_greedy(content, [&](const ast::Markup_Element& c) {
        return out.consume_content(c, frame, context);
    });
}

[[nodiscard]]
Processing_Status splice_value(
    Content_Policy& out,
    const Value& value,
    const File_Source_Span& error_location,
    Context& context
);

enum struct Float_Format : Default_Underlying {
    splice,
    scientific,
    fixed,
};

void splice_bool(Content_Policy& out, bool);
void splice_int(Content_Policy& out, const Big_Int&);
void splice_float(Content_Policy& out, Float, Float_Format format = Float_Format::splice);

[[nodiscard]]
Result<Value, Processing_Status> splice_value_to_string(
    const Value& value,
    const File_Source_Span& error_location,
    Context& context
);

/// @brief Splices the given `value` into the given policy.
/// That is, consumes all elements in a quoted string or block.
/// For primitive spliceable values such as unquoted strings, booleans, etc.,
/// converts the value into its string representation and writes it as plaintext to `out.`
/// @param value The primary.
/// `primary.is_spliceable_value()` shall be `true`.
[[nodiscard]]
Processing_Status splice_expression(
    Content_Policy& out,
    const ast::Expression& value,
    Frame_Index frame,
    Context& context
);

[[nodiscard]]
Processing_Status splice_primary(
    Content_Policy& out, //
    const ast::Primary& primary,
    Frame_Index frame,
    Context& context
);

[[nodiscard]]
Processing_Status splice_value_to_plaintext(
    std::pmr::vector<char8_t>& out, //
    const Value& value,
    const File_Source_Span& error_location,
    Context& context
);

/// @brief Converts a spliceable value to plaintext.
[[nodiscard]]
Processing_Status splice_expression_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Expression& value,
    Frame_Index frame,
    Context& context
);

[[nodiscard]]
inline Processing_Status splice_quoted_string(
    Content_Policy& out,
    const ast::Primary& quoted_string,
    Frame_Index frame,
    Context& context
)
{
    COWEL_ASSERT(quoted_string.get_kind() == ast::Primary_Kind::quoted_string);
    return splice_all(out, quoted_string.get_elements(), frame, context);
}

[[nodiscard]]
inline Processing_Status
splice_block(Content_Policy& out, const ast::Primary& block, Frame_Index frame, Context& context)
{
    COWEL_ASSERT(block.get_kind() == ast::Primary_Kind::block);
    return splice_all(out, block.get_elements(), frame, context);
}

[[nodiscard]]
Result<Value, Processing_Status>
evaluate_expression(const ast::Expression& value, Frame_Index frame, Context& context);

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Directive& directive, Frame_Index frame, Context& context);

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Primary& value, Frame_Index frame, Context& context);

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Unary_Expression& expression, Frame_Index frame, Context& context);

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Binary_Expression& expression, Frame_Index frame, Context& context);

[[nodiscard]]
Result<Value, Processing_Status>
evaluate(const ast::Let_Expression& expression, Frame_Index frame, Context& context);

Result<Value, Processing_Status> evaluate_unary(
    Unary_Expression_Kind kind, //
    const Value& value,
    const File_Source_Span& error_location,
    Context& context
);

/// @brief Checks whether `lhs` and `rhs` are correct types for the given `kind` of expression,
/// and returns the concrete `Builtin_Operation_Kind` if so.
/// Otherwise, returns `Processing_Status::error` and outputs a diagnostic.
[[nodiscard]]
Result<Builtin_Operation_Kind, Processing_Status> check_dynamically_typed_operation(
    Builtin_Operation_Kind dynamic_kind,
    const Type& lhs,
    const Type& rhs,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
);

[[nodiscard]]
Result<Value, Processing_Status> evaluate_builtin(
    Builtin_Operation_Kind kind,
    const Value& lhs,
    const Value& rhs,
    const File_Source_Span& lhs_location,
    const File_Source_Span& rhs_location,
    Context& context
);

[[nodiscard]]
const Type& get_static_type(const ast::Expression& v, Context& context);

/// @brief Returns the static type of a directive,
/// based on its behavior within the `context`.
/// If name lookup fails, returns `Type::any`.
[[nodiscard]]
const Type& get_static_type(const ast::Directive& directive, Context& context);

/// @brief Returns the type of a primary.
/// This is never `any`, but some specific type
/// because primaries are things like boolean literals,
/// whose type is easily determined.
[[nodiscard]]
const Type& get_type(const ast::Primary& primary);

[[nodiscard]]
Processing_Status splice_to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Markup_Element> content,
    Frame_Index frame,
    Context& context
);

struct Plaintext_Result {
    Processing_Status status;
    std::u8string_view string;
};

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

void diagnose(
    Syntax_Highlight_Error error,
    std::u8string_view lang,
    const Invocation& call,
    Context& context
);

Result<void, std::size_t> named_str_arguments_to_attributes(
    Text_Buffer_Attribute_Writer& out,
    std::span<const Group_Member_Value> arguments,
    Attribute_Style style = Attribute_Style::double_if_needed
);

struct Group_Pack_Named_Str_Matcher;
struct Pack_Named_Of_Type_Matcher;

Processing_Status named_arguments_to_attributes_or_error(
    Text_Buffer_Attribute_Writer& out,
    const Group_Pack_Named_Str_Matcher& matcher,
    Context& context,
    Attribute_Style style = Attribute_Style::double_if_needed
);

Processing_Status named_arguments_to_attributes_or_error(
    Text_Buffer_Attribute_Writer& out,
    const Pack_Named_Of_Type_Matcher& matcher,
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

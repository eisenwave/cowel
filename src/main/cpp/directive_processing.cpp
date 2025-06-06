#include <algorithm>
#include <cstddef>
#include <cstring>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "ulight/ulight.hpp"

#include "cowel/fwd.hpp"
#include "cowel/parse_utils.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/from_chars.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/services.hpp"

namespace cowel {

Directive_Behavior* Context::find_directive(std::u8string_view name)
{
    for (const Name_Resolver* const resolver : std::views::reverse(m_name_resolvers)) {
        if (Directive_Behavior* const result = (*resolver)(name, *this)) {
            return result;
        }
    }
    return nullptr;
}

Directive_Behavior* Context::find_directive(const ast::Directive& directive)
{
    return find_directive(directive.get_name());
}

std::span<const ast::Content> trim_blank_text_left(std::span<const ast::Content> content)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.front())) {
            if (is_ascii_blank(text->get_source())) {
                content = content.subspan(1);
                continue;
            }
        }
        if (const auto* const text = std::get_if<ast::Generated>(&content.front())) {
            if (is_ascii_blank(text->as_string())) {
                content = content.subspan(1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content> trim_blank_text_right(std::span<const ast::Content> content)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.back())) {
            if (is_ascii_blank(text->get_source())) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        if (const auto* const text = std::get_if<ast::Generated>(&content.back())) {
            if (is_ascii_blank(text->as_string())) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content> trim_blank_text(std::span<const ast::Content> content)
{
    return trim_blank_text_right(trim_blank_text_left(content));
}

namespace {

void try_lookup_error(const ast::Directive& directive, Context& context)
{
    if (!context.emits(Severity::error)) {
        return;
    }

    // TODO: it would be better to only use the name as the source span,
    //       but this requires a new convenience function in ast::Directive.
    const std::u8string_view message[] {
        u8"No directive with the name \"",
        directive.get_name(),
        u8"\" exists.",
    };
    context.try_error(
        diagnostic::directive_lookup_unresolved, directive.get_source_span(), message
    );
}

void to_plaintext_trimmed(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Context& context
)
{
    content = trim_blank_text(content);

    struct Visitor {
        std::pmr::vector<char8_t>& out;
        std::span<const ast::Content> content;
        Context& context;
        std::size_t i;

        void operator()(const ast::Text& text) const
        {
            std::u8string_view str = text.get_source();
            // Note that the following two conditions are not mutually exclusive
            // when content contains just one element.
            if (i == 0) {
                str = trim_ascii_blank_left(str);
            }
            if (i + 1 == content.size()) {
                str = trim_ascii_blank_right(str);
            }
            // The trimming above should have gotten rid of entirely empty strings.
            COWEL_ASSERT(!str.empty());
            append(out, text.get_source());
        }

        void operator()(const ast::Generated&) const
        {
            COWEL_ASSERT_UNREACHABLE(
                u8"There should be no generated content in a plaintext context."
            );
        }

        void operator()(const ast::Escaped& e) const
        {
            out.push_back(e.get_char());
        }

        void operator()(const ast::Directive& e) const
        {
            to_plaintext(out, e, context);
        }
    };

    for (std::size_t i = 0; i < content.size(); ++i) {
        std::visit(Visitor { out, content, context, i }, content[i]);
    }
}

} // namespace

To_Plaintext_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Content& c,
    Context& context,
    To_Plaintext_Mode mode
)
{
    if (const auto* const t = get_if<ast::Text>(&c)) {
        const std::u8string_view text = t->get_source();
        out.insert(out.end(), text.begin(), text.end());
        return To_Plaintext_Status::ok;
    }
    if (const auto* const e = get_if<ast::Escaped>(&c)) {
        out.push_back(e->get_char());
        return To_Plaintext_Status::ok;
    }
    if (const auto* const b = get_if<ast::Generated>(&c)) {
        if (b->get_type() == ast::Generated_Type::plaintext) {
            append(out, b->as_string());
            return To_Plaintext_Status::ok;
        }
        return To_Plaintext_Status::some_ignored;
    }
    if (const auto* const d = get_if<ast::Directive>(&c)) {
        return to_plaintext(out, *d, context, mode);
    }
    COWEL_ASSERT_UNREACHABLE(u8"Invalid form of content.");
}

To_Plaintext_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context,
    To_Plaintext_Mode mode
)
{
    Directive_Behavior* const behavior = context.find_directive(d);
    if (!behavior) {
        try_lookup_error(d, context);
        try_generate_error_plaintext(out, d, context);
        return To_Plaintext_Status::error;
    }

    switch (behavior->category) {
    case Directive_Category::pure_plaintext: {
        behavior->generate_plaintext(out, d, context);
        return To_Plaintext_Status::ok;
    }
    case Directive_Category::formatting: {
        if (mode == To_Plaintext_Mode::no_side_effects) {
            to_plaintext(out, d.get_content(), context, To_Plaintext_Mode::no_side_effects);
        }
        else {
            behavior->generate_plaintext(out, d, context);
        }
        return To_Plaintext_Status::ok;
    }
    default: {
        if (mode != To_Plaintext_Mode::no_side_effects) {
            behavior->generate_plaintext(out, d, context);
            return To_Plaintext_Status::ok;
        }
        return To_Plaintext_Status::some_ignored;
    }
    }
    COWEL_ASSERT_UNREACHABLE(u8"Should have returned in switch.");
}

To_Plaintext_Status to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Context& context,
    To_Plaintext_Mode mode
)
{
    if (mode == To_Plaintext_Mode::trimmed) {
        to_plaintext_trimmed(out, content, context);
        return To_Plaintext_Status::ok;
    }

    auto result = To_Plaintext_Status::ok;
    for (const ast::Content& c : content) {
        const auto c_result = to_plaintext(out, c, context, mode);
        result = To_Plaintext_Status(std::max(int(result), int(c_result)));
    }
    return result;
}

void to_html(HTML_Writer& out, const ast::Content& c, Context& context)
{
    std::visit([&](const auto& x) { to_html(out, x, context); }, c);
}

void to_html(HTML_Writer& out, const ast::Text& text, [[maybe_unused]] Context& context)
{
    const std::u8string_view output = text.get_source();
    out.write_inner_text(output);
}

void to_html(HTML_Writer& out, const ast::Escaped& escaped, [[maybe_unused]] Context& context)
{
    const char8_t c = escaped.get_char();
    out.write_inner_text(c);
}

void to_html(HTML_Writer& out, const ast::Generated& content, Context&)
{
    switch (content.get_type()) {
    case ast::Generated_Type::plaintext: //
        out.write_inner_text(content.as_string());
        break;
    case ast::Generated_Type::html: //
        out.write_inner_html(content.as_string());
        break;
    }
}

void to_html(HTML_Writer& out, const ast::Directive& directive, Context& context)
{
    if (Directive_Behavior* const behavior = context.find_directive(directive)) {
        behavior->generate_html(out, directive, context);
        return;
    }
    try_lookup_error(directive, context);
    try_generate_error_html(out, directive, context);
}

namespace {

void to_html_direct(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
{
    for (const auto& c : content) {
        to_html(out, c, context);
    }
}

void to_html_trimmed(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
{
    struct Visitor {
        HTML_Writer& out;
        std::span<const ast::Content> content;
        Context& context;
        std::size_t i;

        void operator()(const ast::Text& text) const
        {
            std::u8string_view str = text.get_source();
            // Note that the following two conditions are not mutually exclusive
            // when content contains just one element.
            if (i == 0) {
                str = trim_ascii_blank_left(str);
            }
            if (i + 1 == content.size()) {
                str = trim_ascii_blank_right(str);
            }
            // Other trimming mechanisms should have eliminated completely blank strings.
            COWEL_ASSERT(!str.empty());
            out.write_inner_text(str);
        }

        void operator()(const ast::Generated& generated) const
        {
            std::u8string_view str = generated.as_string();
            if (i == 0) {
                str = trim_ascii_blank_left(str);
            }
            if (i + 1 == content.size()) {
                str = trim_ascii_blank_right(str);
            }
            // Other trimming mechanisms should have eliminated completely blank strings.
            COWEL_ASSERT(!str.empty());
            out.write_inner_html(str);
        }

        void operator()(const ast::Escaped& e) const
        {
            out.write_inner_html(e.get_char());
        }

        void operator()(const ast::Directive& e) const
        {
            to_html(out, e, context);
        }
    };

    for (std::size_t i = 0; i < content.size(); ++i) {
        std::visit(Visitor { out, content, context, i }, content[i]);
    }
}

struct To_HTML_Paragraphs {
private:
    HTML_Writer& m_out;
    Context& m_context;
    Paragraphs_State m_state;

public:
    To_HTML_Paragraphs(HTML_Writer& out, Context& context, Paragraphs_State initial_state)
        : m_out { out }
        , m_context { context }
        , m_state { initial_state }
    {
    }

    //  Some directives split paragraphs, and some are inline.
    //  For example, `\\b{...}` gets displayed inline,
    //  but `\\blockquote` is block content.
    void operator()(const ast::Directive& d)
    {
        if (Directive_Behavior* const behavior = m_context.find_directive(d)) {
            if (behavior->category == Directive_Category::macro) {
                const std::pmr::vector<ast::Content> instance = behavior->instantiate(d, m_context);
                for (const auto& content : instance) {
                    std::visit(*this, content);
                }
            }
            else {
                on_non_macro_directive(*behavior, d);
            }
        }
        else {
            try_lookup_error(d, m_context);
        }
        if (Directive_Behavior* const eb = m_context.get_error_behavior()) {
            on_non_macro_directive(*eb, d);
        }
    }

    // Behaved content can also be inline or block.
    void operator()(const ast::Generated& b)
    {
        transition(b.get_display());
        to_html(m_out, b, m_context);
    }

    // Text is never block content in itself,
    // but blank lines can act as separators between paragraphs.
    void operator()(const ast::Text& t, bool trim_left = false, bool trim_right = false)
    {
        std::u8string_view text = t.get_source();
        if (trim_left) {
            text = trim_ascii_blank_left(text);
        }
        if (trim_right) {
            text = trim_ascii_blank_right(text);
        }
        if (text.empty()) {
            return;
        }

        // We need to consider the special case of a single leading `\n`.
        // This is technically a blank line when it appears at the start of a string,
        // but is irrelevant to forming paragraphs.
        //
        // For example, we could have two `\b{}` directives separated by a single newline.
        // This is a blank line when looking at the contents of the `ast::Text` node,
        // but isn't a blank line within the context of the document.
        if (const Blank_Line blank = find_blank_line_sequence(text);
            blank.begin == 0 && blank.length == 1) {
            m_out.write_inner_text(text[0]);
            text.remove_prefix(1);
        }

        while (!text.empty()) {
            const Blank_Line blank = find_blank_line_sequence(text);
            if (!blank) {
                COWEL_ASSERT(blank.begin == 0);
                transition(Directive_Display::in_line);
                m_out.write_inner_text(text);
                break;
            }

            // If the blank isn't at the start of the text,
            // that means we have some plain character prior to the blank
            // which we need write first.
            if (blank.begin != 0) {
                transition(Directive_Display::in_line);
                m_out.write_inner_text(text.substr(0, blank.begin));
                text.remove_prefix(blank.begin);
                COWEL_ASSERT(text.length() >= blank.length);
            }
            transition(Directive_Display::block);
            m_out.write_inner_text(text.substr(0, blank.length));
            text.remove_prefix(blank.length);
        }
    }

    // Escape sequences are always inline; they're just a single character.
    void operator()(const ast::Escaped& e)
    {
        transition(Directive_Display::in_line);
        to_html(m_out, e, m_context);
    }

    void flush()
    {
        transition(Directive_Display::block);
    }

private:
    void transition(Directive_Display display)
    {
        switch (display) {
        case Directive_Display::none: {
            return;
        }
        case Directive_Display::in_line: {
            if (m_state == Paragraphs_State::outside && display == Directive_Display::in_line) {
                m_out.open_tag(u8"p");
                m_state = Paragraphs_State::inside;
            }
            return;
        }
        case Directive_Display::block: {
            if (m_state == Paragraphs_State::inside && display == Directive_Display::block) {
                m_out.close_tag(u8"p");
                m_state = Paragraphs_State::outside;
            }
            return;
        }
        case Directive_Display::macro: {
            COWEL_ASSERT_UNREACHABLE(u8"Macros should have been instantiated already.");
            break;
        }
        }
        COWEL_ASSERT_UNREACHABLE(u8"Invalid display value.");
    }

    void on_non_macro_directive(Directive_Behavior& b, const ast::Directive& d)
    {
        transition(b.display);
        b.generate_html(m_out, d, m_context);
    }
};

} // namespace

void to_html(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    Context& context,
    To_HTML_Mode mode,
    Paragraphs_State paragraphs_state
)
{
    if (to_html_mode_is_trimmed(mode)) {
        content = trim_blank_text(content);
    }

    switch (mode) {
    case To_HTML_Mode::direct: //
        to_html_direct(out, content, context);
        break;

    case To_HTML_Mode::trimmed: //
        to_html_trimmed(out, content, context);
        break;

    case To_HTML_Mode::paragraphs:
    case To_HTML_Mode::paragraphs_trimmed: {
        To_HTML_Paragraphs impl { out, context, paragraphs_state };

        for (std::size_t i = 0; i < content.size(); ++i) {
            if (mode == To_HTML_Mode::paragraphs_trimmed) {
                if (const auto* const text = std::get_if<ast::Text>(&content[i])) {
                    const bool first = i == 0;
                    const bool last = i + 1 == content.size();
                    impl(*text, first, last);
                    continue;
                }
            }
            std::visit(impl, content[i]);
        }
        impl.flush();
        break;
    }
    }
}

void to_html_literally(HTML_Writer& out, std::span<const ast::Content> content, Context&)
{
    for (const ast::Content& c : content) {
        if (const auto* const e = get_if<ast::Escaped>(&c)) {
            out.write_inner_html(e->get_char());
        }
        if (const auto* const t = get_if<ast::Text>(&c)) {
            out.write_inner_html(t->get_source());
        }
        if (const auto* const _ = get_if<ast::Generated>(&c)) {
            COWEL_ASSERT_UNREACHABLE(u8"Attempting to generate literal HTML from Behaved_Content");
            return;
        }
        if (const auto* const d = get_if<ast::Directive>(&c)) {
            out.write_inner_text(d->get_source());
        }
    }
}

namespace {

struct Index_Range {
    std::size_t begin;
    std::size_t length;

    [[nodiscard]]
    constexpr std::size_t end() const
    {
        return begin + length;
    }

    [[nodiscard]]
    constexpr bool empty() const
    {
        return length == 0;
    }

    [[nodiscard]]
    constexpr bool intersects(Index_Range other) const
    {
        return begin < other.end() && other.begin < end();
    }
};

[[maybe_unused]]
constexpr char32_t private_use_area_min
    = 0xE000;
[[maybe_unused]]
constexpr char32_t private_use_area_max
    = 0xF8FF;

void reference_highlighted(HTML_Writer& out, std::size_t begin, std::size_t length)
{
    using Array = std::array<char8_t, sizeof(Index_Range)>;

    out.write_inner_html(private_use_area_min);
    const Index_Range range { begin, length };
    const auto chars = std::bit_cast<Array>(range);
    out.write_inner_html(as_u8string_view(chars));
}

void to_html_with_source_references(
    HTML_Writer& out,
    std::pmr::vector<char8_t>& out_code,
    const ast::Content& content,
    Context& context
);

/// @brief Like to_html,
/// but highlightable content is is not directly written to out;
/// instead, highlightable content is stored inside of `out_code` and an `Index_Range`
/// is encoded within `out`,
/// where the index range stores the range of code that has been written to `out_code`.
///
/// This process allows running all the highlightable content through a syntax highlighter
/// and replacing the source code references in a post-processing pass.
///
/// The following content is highlightable:
/// - plaintext
/// - escape sequences
/// - the text produced by plaintext directives
/// - the contents of formatting directives
/// - any of the above, expanded from macros
///
/// Any non-highlightable content is converted to HTML as usual.
void to_html_with_source_references(
    HTML_Writer& out,
    std::pmr::vector<char8_t>& out_code,
    std::span<const ast::Content> content,
    Context& context
)
{
    for (const ast::Content& c : content) {
        to_html_with_source_references(out, out_code, c, context);
    }
}

void to_html_with_source_references(
    HTML_Writer& out,
    std::pmr::vector<char8_t>&,
    const ast::Generated& generated,
    [[maybe_unused]] Context& context
)
{
    out.write_inner_html(generated.as_string());
}

void to_html_with_source_references(
    HTML_Writer& out,
    std::pmr::vector<char8_t>& out_code,
    const ast::Text& t,
    [[maybe_unused]] Context& context
)
{
    const std::size_t initial_size = out_code.size();
    const std::u8string_view text = t.get_source();
    COWEL_ASSERT(!text.empty());
    out_code.insert(out_code.end(), text.begin(), text.end());
    reference_highlighted(out, initial_size, text.length());
}

void to_html_with_source_references(
    HTML_Writer& out,
    std::pmr::vector<char8_t>& out_code,
    const ast::Escaped& e,
    [[maybe_unused]] Context& context
)
{
    const std::size_t initial_size = out_code.size();
    out_code.push_back(e.get_char());
    reference_highlighted(out, initial_size, 1);
}

void to_html_with_source_references(
    HTML_Writer& out,
    std::pmr::vector<char8_t>& out_code,
    const ast::Directive& d,
    Context& context
)
{
    Directive_Behavior* const behavior = context.find_directive(d);
    if (!behavior) {
        return;
    }
    switch (behavior->category) {
    case Directive_Category::meta:
    case Directive_Category::pure_html: {
        behavior->generate_html(out, d, context);
        break;
    }
    case Directive_Category::formatting: {
        auto generated = [&] {
            std::pmr::vector<char8_t> generated_html { context.get_transient_memory() };
            HTML_Writer generated_writer { generated_html };
            to_html_with_source_references(generated_writer, out_code, d.get_content(), context);
            return ast::Generated { std::move(generated_html), ast::Generated_Type::html,
                                    Directive_Display::in_line };
        }();
        // TODO: this could be sped up if we don't clone all th content just to clear() it
        auto clone = d;
        clone.get_content().clear();
        clone.get_content().push_back(std::move(generated));
        behavior->generate_html(out, clone, context);
        break;
    }
    case Directive_Category::macro: {
        const std::pmr::vector<ast::Content> instance = behavior->instantiate(d, context);
        to_html_with_source_references(out, out_code, instance, context);
        break;
    }
    case Directive_Category::pure_plaintext: {
        const std::size_t initial_size = out_code.size();
        behavior->generate_plaintext(out_code, d, context);
        COWEL_ASSERT(out_code.size() >= initial_size);
        const std::size_t length = out_code.size() - initial_size;
        reference_highlighted(out, initial_size, length);
        break;
    }
    }
}

void to_html_with_source_references(
    HTML_Writer& out,
    std::pmr::vector<char8_t>& out_code,
    const ast::Content& content,
    Context& context
)
{
    const auto visitor = [&](const auto& c) { //
        to_html_with_source_references(out, out_code, c, context);
    };
    std::visit(visitor, content);
}

constexpr std::u8string_view highlighting_tag = u8"h-";
constexpr std::u8string_view highlighting_attribute = u8"data-h";
constexpr auto highlighting_attribute_style = Attribute_Style::double_if_needed;

/// @brief Writes HTML containing syntax highlighting elements to `out`.
/// @param out The writer.
/// @param code_range The range of indices within the given source.
/// @param code The highlighted source code.
/// @param highlights The highlights for `source`.
void generate_highlighted_html(
    HTML_Writer& out,
    Index_Range code_range,
    std::u8string_view code,
    std::span<const Highlight_Span> highlights
)
{
    COWEL_ASSERT(!code_range.empty());
    COWEL_ASSERT(code_range.begin < code.length());
    COWEL_ASSERT(code_range.begin + code_range.length <= code.length());

    constexpr auto projection
        = [](const Highlight_Span& highlight) { return highlight.begin + highlight.length; };
    auto it = std::ranges::upper_bound(highlights, code_range.begin, {}, projection);
    std::size_t index = code_range.begin;

    for (; it != highlights.end() && code_range.intersects({ it->begin, it->length }); ++it) {
        const Highlight_Span& highlight = *it;
        COWEL_ASSERT(highlight.begin < code.length());
        COWEL_ASSERT(highlight.begin + highlight.length <= code.length());

        // Leading non-highlighted content.
        if (highlight.begin > index) {
            out.write_inner_text(code.substr(index, highlight.begin - index));
            index = highlight.begin;
        }
        // This length limit is necessary because it is possible that the source reference ends
        // in the middle of a highlight, like:
        //     \i{in}t x = 0
        // where the keyword highlight for "int" would extend further than the reference for "in".
        const std::size_t actual_end
            = std::min(code_range.end(), highlight.begin + highlight.length);
        if (index >= actual_end) {
            // TODO: investigate whether this is actually possible;
            //       I suspect this could be an assertion.
            break;
        }

        const std::u8string_view id
            = ulight::highlight_type_short_string_u8(ulight::Highlight_Type(highlight.type));
        out.open_tag_with_attributes(highlighting_tag)
            .write_attribute(highlighting_attribute, id, highlighting_attribute_style)
            .end();
        out.write_inner_text(code.substr(index, actual_end - index));
        out.close_tag(highlighting_tag);
        index = actual_end;
    }

    // Trailing non-highlighted content, but still within the code range.
    const std::size_t code_range_end = code_range.begin + code_range.length;
    COWEL_ASSERT(index <= code_range_end);
    if (index < code_range_end) {
        out.write_inner_text(code.substr(index, code_range_end - index));
    }
}

/// @brief Resolves references to syntax-highlighted code within the given `generated` markup.
/// @param generated The generated HTML, possibly containing source code references.
/// @param start The first index in `generated` which may contain source code references.
/// @param code The highlighted source code.
/// @param highlights The highlights for `code`.
void resolve_source_references(
    std::pmr::vector<char8_t>& generated,
    const std::size_t start,
    const std::u8string_view code,
    const std::span<const Highlight_Span> highlights
)
{
    COWEL_ASSERT(start <= generated.size());
    std::pmr::vector<char8_t> buffer { generated.get_allocator() };

    for (std::size_t i = start; i < generated.size();) {
        const auto remainder_string = as_u8string_view(generated).substr(i);
        const auto [code_point, length] = utf8::decode_and_length_or_throw(remainder_string);
        COWEL_ASSERT(length > 0);
        if (code_point < private_use_area_min || code_point > private_use_area_max) {
            i += std::size_t(length);
            continue;
        }
        COWEL_ASSERT(code_point == private_use_area_min);
        COWEL_ASSERT(i + std::size_t(length) <= generated.size());

        Index_Range range;
        std::memcpy(&range, generated.data() + i + length, sizeof(range));

        buffer.clear();
        HTML_Writer buffer_writer { buffer };
        generate_highlighted_html(buffer_writer, range, code, highlights);

        const auto post_erased = generated.erase(
            generated.begin() + std::ptrdiff_t(i),
            generated.begin() + std::ptrdiff_t(i) + length + sizeof(range)
        );
        generated.insert(post_erased, buffer.begin(), buffer.end());
        i += buffer.size();
    }
}

} // namespace

Result<void, Syntax_Highlight_Error> to_html_syntax_highlighted(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    std::u8string_view language,
    Context& context,
    std::u8string_view prefix,
    std::u8string_view suffix
)
{
    const std::size_t initial_size = out.get_output().size();

    std::pmr::vector<char8_t> code { context.get_transient_memory() };
    code.insert(code.end(), prefix.begin(), prefix.end());
    to_html_with_source_references(out, code, content, context);
    code.insert(code.end(), suffix.begin(), suffix.end());

    const auto code_string = as_u8string_view(code);

    Syntax_Highlighter& highlighter = context.get_highlighter();
    std::pmr::vector<Highlight_Span> highlights { context.get_transient_memory() };
    Result<void, Syntax_Highlight_Error> result
        = highlighter(highlights, code_string, language, context.get_transient_memory());
    // Even if the result is an error,
    // we need to carry on as usual and resolve all the references,
    // which will simply be considered references to non-highlighted content and emit
    // as if highlighting was never attempted.
    resolve_source_references(out.get_output(), initial_size, code_string, highlights);
    return result;
}

void warn_ignored_argument_subset(
    std::span<const ast::Argument> args,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset ignored_subset
)
{
    const std::span<const Argument_Status> statuses = matcher.argument_statuses();
    COWEL_ASSERT(args.size() == statuses.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        const bool is_matched = statuses[i] != Argument_Status::unmatched;
        const bool is_named = arg.has_name();
        const Argument_Subset subset = argument_subset_matched_named(is_matched, is_named);
        if (argument_subset_contains(ignored_subset, subset)) {
            context.try_warning(
                diagnostic::ignored_args, arg.get_source_span(), u8"This argument was ignored."
            );
        }
    }
}

void warn_ignored_argument_subset(
    std::span<const ast::Argument> args,
    Context& context,
    Argument_Subset ignored_subset
)
{
    COWEL_ASSERT(
        argument_subset_contains(ignored_subset, Argument_Subset::matched)
        == argument_subset_contains(ignored_subset, Argument_Subset::unmatched)
    );

    for (const auto& arg : args) {
        const auto subset = arg.has_name() ? Argument_Subset::named : Argument_Subset::positional;
        if (argument_subset_contains(ignored_subset, subset)) {
            context.try_warning(
                diagnostic::ignored_args, arg.get_source_span(), u8"This argument was ignored."
            );
        }
    }
}

void named_arguments_to_attributes(
    Attribute_Writer& out,
    const ast::Directive& d,
    Context& context,
    Function_Ref<bool(std::u8string_view)> filter,
    Attribute_Style style
)
{
    const std::span<const ast::Argument> args = d.get_arguments();
    for (std::size_t i = 0; i < args.size(); ++i) {
        const ast::Argument& a = args[i];
        if (!a.has_name()) {
            continue;
        }
        bool duplicate = false;
        for (std::size_t j = 0; j < i; ++j) {
            if (args[j].get_name() == a.get_name()) {
                const std::u8string_view message[]
                    = { u8"This argument is a duplicate of a previous named argument also named \"",
                        args[j].get_name(), u8"\", and will be ignored." };
                context.try_warning(diagnostic::duplicate_args, args[j].get_source_span(), message);
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            named_argument_to_attribute(out, a, context, filter, style);
        }
    }
}

void named_arguments_to_attributes(
    Attribute_Writer& out,
    const ast::Directive& d,
    const Argument_Matcher& matcher,
    Context& context,
    Argument_Subset subset,
    Attribute_Style style
)
{
    COWEL_ASSERT(!argument_subset_intersects(subset, Argument_Subset::positional));

    named_arguments_to_attributes(
        out, d, context,
        [&](std::u8string_view name) {
            const int index = matcher.get_argument_index(name);
            const auto arg_subset
                = index < 0 ? Argument_Subset::unmatched_named : Argument_Subset::matched_named;
            return argument_subset_contains(subset, arg_subset);
        },
        style
    );
}

bool named_argument_to_attribute(
    Attribute_Writer& out,
    const ast::Argument& a,
    Context& context,
    Function_Ref<bool(std::u8string_view)> filter,
    Attribute_Style style
)
{
    COWEL_ASSERT(a.has_name());
    std::pmr::vector<char8_t> value { context.get_transient_memory() };
    // TODO: error handling
    value.clear();
    to_plaintext(value, a.get_content(), context);
    const std::u8string_view value_string { value.data(), value.size() };
    const std::u8string_view name = a.get_name();
    if (!filter || filter(name)) {
        out.write_attribute(name, value_string, style);
        return true;
    }
    return false;
}

bool argument_to_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    const Argument_Matcher& args,
    std::u8string_view parameter,
    Context& context
)
{
    const int i = args.get_argument_index(parameter);
    if (i < 0) {
        return false;
    }
    const ast::Argument& arg = d.get_arguments()[std::size_t(i)];
    // TODO: warn when pure HTML argument was used as variable name
    to_plaintext(out, arg.get_content(), context);
    return true;
}

bool get_yes_no_argument(
    std::u8string_view name,
    std::u8string_view diagnostic_id,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    bool fallback
)
{
    const int index = args.get_argument_index(name);
    if (index < 0) {
        return fallback;
    }
    const ast::Argument& arg = d.get_arguments()[std::size_t(index)];
    std::pmr::vector<char8_t> data { context.get_transient_memory() };
    to_plaintext(data, arg.get_content(), context);
    const auto string = as_u8string_view(data);
    if (string == u8"yes") {
        return true;
    }
    if (string == u8"no") {
        return false;
    }
    const std::u8string_view message[] {
        u8"Argument has to be \"yes\" or \"no\", but \"",
        string,
        u8"\" was given.",
    };
    context.try_warning(diagnostic_id, arg.get_source_span(), message);
    return fallback;
}

std::size_t get_integer_argument(
    std::u8string_view name,
    std::u8string_view parse_error_diagnostic,
    std::u8string_view range_error_diagnostic,
    const Argument_Matcher& args,
    const ast::Directive& d,
    Context& context,
    std::size_t fallback,
    std::size_t min,
    std::size_t max
)
{
    COWEL_ASSERT(fallback >= min && fallback <= max);

    const int index = args.get_argument_index(name);
    if (index < 0) {
        return fallback;
    }
    const ast::Argument& arg = d.get_arguments()[std::size_t(index)];
    std::pmr::vector<char8_t> arg_text { context.get_transient_memory() };
    to_plaintext(arg_text, arg.get_content(), context);
    const auto arg_string = as_u8string_view(arg_text);

    const std::optional<std::size_t> value = from_chars<std::size_t>(arg_string);
    if (!value) {
        const std::u8string_view message[] {
            u8"The specified ",
            name,
            u8" \"",
            arg_string,
            u8"\" is ignored because it could not be parsed as a (positive) integer.",
        };
        context.try_warning(parse_error_diagnostic, arg.get_source_span(), message);
        return fallback;
    }
    if (value < min || value > max) {
        const Characters8 min_chars = to_characters8(min);
        const Characters8 max_chars = to_characters8(max);
        const std::u8string_view message[] {
            u8"The specified ",
            name,
            u8" \"",
            arg_string,
            u8"\" is ignored because it is outside of the valid range [",
            min_chars.as_string(),
            u8", ",
            max_chars.as_string(),
            u8"].",
        };
        context.try_warning(range_error_diagnostic, arg.get_source_span(), message);
        return fallback;
    }

    return *value;
}

namespace {

[[nodiscard]]
std::u8string_view argument_to_plaintext_or(
    std::pmr::vector<char8_t>& out,
    std::u8string_view parameter_name,
    std::u8string_view fallback,
    const ast::Directive& directive,
    const Argument_Matcher& args,
    Context& context
)
{
    const int index = args.get_argument_index(parameter_name);
    if (index < 0) {
        return fallback;
    }
    to_plaintext(out, directive.get_arguments()[std::size_t(index)].get_content(), context);
    return { out.data(), out.size() };
}

} // namespace

String_Argument get_string_argument(
    std::u8string_view name,
    const ast::Directive& d,
    const Argument_Matcher& args,
    Context& context,
    std::u8string_view fallback
)
{
    String_Argument result { .data = std::pmr::vector<char8_t>(context.get_transient_memory()),
                             .string = {} };
    result.string = argument_to_plaintext_or(result.data, name, fallback, d, args, context);
    return result;
}

void try_generate_error_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
)
{
    if (const Directive_Behavior* const behavior = context.get_error_behavior()) {
        behavior->generate_plaintext(out, d, context);
    }
}

void try_generate_error_html(HTML_Writer& out, const ast::Directive& d, Context& context)
{
    if (const Directive_Behavior* const behavior = context.get_error_behavior()) {
        behavior->generate_html(out, d, context);
    }
}

} // namespace cowel

#include <cstddef>
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
#include "cowel/util/source_position.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/services.hpp"

namespace cowel {

Directive_Behavior* Context::find_directive(std::u8string_view name) const
{
    for (const Name_Resolver* const resolver : std::views::reverse(m_name_resolvers)) {
        if (Directive_Behavior* const result = (*resolver)(name)) {
            return result;
        }
    }
    return nullptr;
}

Directive_Behavior* Context::find_directive(const ast::Directive& directive) const
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

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char8_t>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Content& c,
    Context& context
)
{
    std::visit(
        [&]<typename T>(const T& x) {
            if constexpr (std::is_same_v<T, ast::Generated>) {
                COWEL_ASSERT_UNREACHABLE(u8"Generated content during syntax highlighting?!");
            }
            else {
                to_plaintext_mapped_for_highlighting(out, out_mapping, x, context);
            }
        },
        c
    );
}

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char8_t>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Text& t,
    [[maybe_unused]] Context& context
)
{
    // TODO: to be accurate, we would have to process HTML entities here so that syntax highlighting
    //       sees them as a character rather than attempting to highlight the original entity.
    //       For example, `&lt;` should be highlighted like a `<` operator.
    const std::u8string_view text = t.get_source();
    out.insert(out.end(), text.begin(), text.end());

    const Source_Span pos = t.get_source_span();
    COWEL_ASSERT(pos.length == text.length());

    const std::size_t initial_size = out_mapping.size();
    out_mapping.reserve(initial_size + pos.length);
    for (std::size_t i = pos.begin; i < pos.end(); ++i) {
        out_mapping.push_back(i);
    }
    COWEL_ASSERT(out_mapping.size() - initial_size == text.size());
}

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char8_t>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Escaped& e,
    [[maybe_unused]] Context& context
)
{
    out.push_back(e.get_char());
    out_mapping.push_back(e.get_char_index());
}

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char8_t>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Directive& d,
    Context& context
)
{
    Directive_Behavior* const behavior = context.find_directive(d);
    if (!behavior) {
        return;
    }
    switch (behavior->category) {
    // Meta directives such as comments cannot generate plaintext anyway.
    case Directive_Category::meta:
    // Pure HTML directives don't interoperate with syntax highlighting at all.
    case Directive_Category::pure_html: {
        break;
    }
    // Formatting directives such as `\b` are very special;
    // it is guaranteed that we can apply syntax highlighting to the content within,
    // and feed that back into the formatting directive.
    //
    // In this function, we just recurse into the directive's contents so we know which piece of
    // content within produced what syntax-highlighted part.
    case Directive_Category::formatting: {
        to_plaintext_mapped_for_highlighting(out, out_mapping, d.get_content(), context);
        break;
    }
    // For macro expansions, text or directives inside the macro
    // are not actually within the source highlighting block.
    // However, that's not really a problem; subsequent functionality can deal with that.
    case Directive_Category::macro: {
        const std::pmr::vector<ast::Content> instance = behavior->instantiate(d, context);
        to_plaintext_mapped_for_highlighting(out, out_mapping, instance, context);
        break;
    }
    // For pure plaintext directives, we just run plaintext generation.
    // This also means that we don't know exactly which generated character belongs to
    // which source character, but it doesn't really matter.
    // We never run HTML generation afterwards and substitute the plaintext directive
    // with various syntax-highlighted content.
    case Directive_Category::pure_plaintext: {
        const std::size_t initial_out_size = out.size();
        const std::size_t initial_mapping_size = out_mapping.size();
        behavior->generate_plaintext(out, d, context);
        COWEL_ASSERT(out.size() >= initial_out_size);
        const std::size_t out_growth = out.size() - initial_out_size;
        out_mapping.reserve(out_mapping.size() + out_growth);
        const std::size_t d_begin = d.get_source_span().begin;
        for (std::size_t i = initial_out_size; i < out.size(); ++i) {
            out_mapping.push_back(d_begin);
        }
        const std::size_t mapping_growth = out_mapping.size() - initial_mapping_size;
        COWEL_ASSERT(out_growth == mapping_growth);
        break;
    }
    }
}

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char8_t>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    std::span<const ast::Content> content,
    Context& context
)
{
    for (const ast::Content& c : content) {
        to_plaintext_mapped_for_highlighting(out, out_mapping, c, context);
    }
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
            text = trim_ascii_blank_left(text);
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

constexpr std::u8string_view highlighting_tag = u8"h-";

/// @brief Returns an `ast::Generated` element
/// containing a highlighting element or simply the given text,
/// depending on whether `span` is null.
[[nodiscard]]
ast::Generated make_generated_highlight(
    std::u8string_view inner_text,
    const Highlight_Span* span,
    std::pmr::memory_resource* memory
)
{
    std::pmr::vector<char8_t> span_data { memory };
    HTML_Writer span_writer { span_data };

    if (span) {
        const std::u8string_view id
            = ulight::highlight_type_short_string_u8(ulight::Highlight_Type(span->type));

        span_writer.open_tag_with_attributes(highlighting_tag)
            .write_attribute(u8"data-h", id, Attribute_Style::double_if_needed)
            .end();
    }
    span_writer.write_inner_text(inner_text);
    if (span) {
        span_writer.close_tag(highlighting_tag);
    }

    return ast::Generated { std::move(span_data), ast::Generated_Type::html,
                            Directive_Display::in_line };
}

std::pmr::vector<ast::Content> copy_highlighted(
    std::span<const ast::Content> content,
    std::u8string_view highlighted_text,
    std::span<const std::size_t> to_source_index,
    std::span<const Highlight_Span*> to_highlight_span,
    Context& context
);

struct [[nodiscard]] Highlighted_AST_Copier {
    std::pmr::vector<ast::Content>& out;

    const std::u8string_view source;
    const std::span<const std::size_t> to_document_index;
    const std::span<const Highlight_Span*> to_highlight;
    Context& context;

    std::size_t index = 0;

    void operator()(const ast::Escaped& e)
    {
        append_highlighted_text_in(e.get_source_span());
    }

    void operator()(const ast::Text& t)
    {
        append_highlighted_text_in(t.get_source_span());
    }

    void operator()(const ast::Generated&)
    {
        COWEL_ASSERT_UNREACHABLE(u8"Generated content during highlighting?");
    }

    void operator()(const ast::Directive& directive)
    {
        const Directive_Behavior* const behavior = context.find_directive(directive);
        if (!behavior) {
            // Lookup is going to fail again later,
            // but we don't care about that while we're performing AST copies yet.
            // Remember that we are not doing generation (and therefore processing).
            out.push_back(directive);
            return;
        }
        switch (behavior->category) {
        case Directive_Category::meta:
        case Directive_Category::pure_html: {
            // Boring cases.
            // These kinds of directives don't participate in syntax highlighting.
            out.push_back(directive);
            return;
        }
        case Directive_Category::pure_plaintext: {
            // Pure plaintext directives should have already been processed previously,
            // so their output is actually present within the highlighted source.
            // Furthermore, they are "pure" in the sense that they can have no side effects,
            // so they can be processed in any order, or not processed at all, but replaced with
            // the equivalent output.
            // For that reason, we can simply treat these directives as if they were text.
            append_highlighted_text_in(directive.get_source_span());
            return;
        }
        case Directive_Category::formatting: {
            // Formatting directive are the most crazy and special in how they're handled here.
            // Formatting directives promise that their contents can be manipulated at will,
            // i.e. they are "transparent to syntax highlighting".
            // Therefore, we apply AST copying recursively within the directive,
            // and synthesize a new formatting directive.

            std::pmr::vector<ast::Content> inner_content;
            Highlighted_AST_Copier inner_copier { .out = inner_content,
                                                  .source = source,
                                                  .to_document_index = to_document_index,
                                                  .to_highlight = to_highlight,
                                                  .context = context,
                                                  .index = index };
            for (const auto& c : directive.get_content()) {
                std::visit(inner_copier, c);
            }
            COWEL_ASSERT(inner_copier.index >= index);
            index = inner_copier.index;

            std::pmr::vector<ast::Argument> copied_arguments { directive.get_arguments().begin(),
                                                               directive.get_arguments().end(),
                                                               context.get_transient_memory() };

            out.push_back(ast::Directive { directive.get_source_span(), directive.get_source(),
                                           directive.get_name(), std::move(copied_arguments),
                                           std::move(inner_content) });
            return;
        }
        case Directive_Category::macro: {
            const std::pmr::vector<ast::Content> instance
                = behavior->instantiate(directive, context);
            for (const auto& content : instance) {
                std::visit(*this, content);
            }
        }
        }
    }

private:
    void append_highlighted_text_in(const Source_Span& document_span)
    {
        COWEL_DEBUG_ASSERT(source.size() == to_document_index.size());
        COWEL_DEBUG_ASSERT(source.size() == to_highlight.size());

        const std::size_t limit = source.size();
        for (std::size_t index = 0; index < limit;) {
            // TODO: There are a few possible optimizations here.
            //       Firstly, we can early-quit once we're past the end of the contiguous
            //       highlighted snippets that fall into document_span.
            //       Secondly, instead of starting the search at zero,
            //       we can use a hint and later restart if nothing was found.
            //       That is because it's very likely that two document spans and their generated
            //       source are contiguous.
            //       Macros break this mold.
            if (!document_span.contains(to_document_index[index])) {
                ++index;
                continue;
            }
            const Highlight_Span* const current_span = to_highlight[index];
            const std::size_t snippet_begin = index++;
            for (; index < limit; ++index) {
                if (!document_span.contains(to_document_index[index])
                    || to_highlight[index] != current_span) {
                    break;
                }
            };
            out.push_back(make_generated_highlight(
                source.substr(snippet_begin, index - snippet_begin), current_span,
                context.get_transient_memory()
            ));
        }
    }
};

/// @brief Creates a copy of the given `content`
// using the specified syntax highlighting information.
/// @param content The content to copy.
/// @param highlighted_text The highlighted source code.
/// @param to_source_index A mapping of each code unit in `highlighted_source`
/// to the index in the document source code.
/// @param to_highlight_span A mapping of each code unit in `highlighted_source`
/// to a pointer to the highlighting span,
/// or to a null pointer if that part of `highlighted_source` is not highlighted.
/// @param context The context.
/// @returns A new vector of `ast::Content`,
/// where text and escape sequences are replaced with `Behaved_Content`
/// wherever syntax highlighting information appears.
/// Furthermore, `pure_plaintext` directives are replaced the same way as text,
/// and the contents of `formatting` directives are replaced, recursively.
std::pmr::vector<ast::Content> copy_highlighted(
    std::span<const ast::Content> content,
    std::u8string_view highlighted_text,
    std::span<const std::size_t> to_source_index,
    std::span<const Highlight_Span*> to_highlight_span,
    Context& context
)
{
    COWEL_ASSERT(to_source_index.size() == highlighted_text.size());
    COWEL_ASSERT(to_highlight_span.size() == highlighted_text.size());

    std::pmr::vector<ast::Content> result { context.get_transient_memory() };
    result.reserve(content.size());

    Highlighted_AST_Copier copier { .out = result,
                                    .source = highlighted_text,
                                    .to_document_index = to_source_index,
                                    .to_highlight = to_highlight_span,
                                    .context = context };

    for (const auto& c : content) {
        std::visit(copier, c);
    }

    return result;
}

} // namespace

Result<void, Syntax_Highlight_Error> to_html_syntax_highlighted(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    std::u8string_view language,
    Context& context,
    std::u8string_view prefix,
    std::u8string_view suffix,
    To_HTML_Mode mode
)
{
    COWEL_ASSERT(!to_html_mode_is_paragraphed(mode));

    std::pmr::vector<char8_t> plaintext { context.get_transient_memory() };
    std::pmr::vector<std::size_t> plaintext_to_document_index { context.get_transient_memory() };

    // 0 is a safe value for document index mapping
    // because it's impossible to provide input to syntax highlighting at the zeroth index.
    plaintext.insert(plaintext.end(), prefix.begin(), prefix.end());
    plaintext_to_document_index.resize(prefix.size(), 0);
    to_plaintext_mapped_for_highlighting(plaintext, plaintext_to_document_index, content, context);
    plaintext.insert(plaintext.end(), suffix.begin(), suffix.end());
    plaintext_to_document_index.resize(plaintext_to_document_index.size() + suffix.size(), 0);
    COWEL_ASSERT(plaintext.size() == plaintext_to_document_index.size());

    std::pmr::vector<Highlight_Span> spans { context.get_transient_memory() };
    const std::u8string_view plaintext_str { plaintext.data(), plaintext.size() };

    Syntax_Highlighter& highlighter = context.get_highlighter();
    const Result<void, Syntax_Highlight_Error> result
        = highlighter(spans, plaintext_str, language, context.get_transient_memory());
    if (!result) {
        return result.error();
    }

    std::pmr::vector<const Highlight_Span*> plaintext_to_span { context.get_transient_memory() };
    plaintext_to_span.resize(plaintext.size());
    for (const Highlight_Span& span : spans) {
        for (std::size_t i = 0; i < span.length; ++i) {
            plaintext_to_span[i + span.begin] = &span;
        }
    }

    const std::pmr::vector<ast::Content> highlighted_content = copy_highlighted(
        content, plaintext_str, plaintext_to_document_index, plaintext_to_span, context
    );
    to_html(out, highlighted_content, context, mode);
    return {};
}

// Code stash:
// The process for syntax highlighting is relatively complicated because it accounts for directives
// that interleave with the highlighted content.
// However, for the common case of highlighted content that contains no directives,
// we could simplify and generate directly.
#if 0 // NOLINT
const HLJS_Annotation_Span* previous_span = nullptr;
for (; index < to_source_index.size(); ++index) {
    const std::size_t source_index = to_source_index[index];
    if (source_index < source_span.begin) {
        continue;
    }
    if (source_index >= source_span.end()) {
        break;
    }
    if (previous_span != to_span[index]) {
        if (previous_span) {
            out.close_tag(highlighting_tag);
        }
        if (to_span[index]) {
            out.open_tag_with_attributes(highlighting_tag)
                .write_attribute(
                    u8"class", hljs_scope_css_classes(to_span[index]->value),
                    Attribute_Style::always_double
                )
                .end();
            previous_span = to_span[index];
        }
    }
    out.write_inner_text(source[index]);
}
if (previous_span != nullptr) {
    out.close_tag(highlighting_tag);
}
#endif

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

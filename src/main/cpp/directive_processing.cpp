#include <cstddef>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "mmml/fwd.hpp"
#include "mmml/parse_utils.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/hljs_scope.hpp"
#include "mmml/util/html_writer.hpp"
#include "mmml/util/result.hpp"
#include "mmml/util/source_position.hpp"
#include "mmml/util/strings.hpp"

#include "mmml/ast.hpp"
#include "mmml/context.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/directives.hpp"
#include "mmml/services.hpp"

namespace mmml {

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
    return find_directive(directive.get_name(m_source));
}

std::span<const ast::Content>
trim_blank_text_left(std::span<const ast::Content> content, Context& context)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.front())) {
            if (is_ascii_blank(text->get_text(context.get_source()))) {
                content = content.subspan(1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content>
trim_blank_text_right(std::span<const ast::Content> content, Context& context)
{
    while (!content.empty()) {
        if (const auto* const text = std::get_if<ast::Text>(&content.back())) {
            if (is_ascii_blank(text->get_text(context.get_source()))) {
                content = content.subspan(0, content.size() - 1);
                continue;
            }
        }
        break;
    }
    return content;
}

std::span<const ast::Content>
trim_blank_text(std::span<const ast::Content> content, Context& context)
{
    return trim_blank_text_right(trim_blank_text_left(content, context), context);
}

namespace {

void try_lookup_error(const ast::Directive& directive, Context& context)
{
    context.try_error(
        u8"directive_lookup.unresolved", directive.get_source_span(),
        u8"No directive with this name exists."
    );
}

} // namespace

/// @brief Converts content to plaintext.
/// For text, this outputs that text literally.
/// For escaped characters, this outputs the escaped character.
/// For directives, this runs `generate_plaintext` using the behavior of that directive,
/// looked up via context.
/// @param context the current processing context
void to_plaintext(std::pmr::vector<char8_t>& out, const ast::Content& c, Context& context)
{
    if (const auto* const t = get_if<ast::Text>(&c)) {
        const std::u8string_view text = t->get_text(context.get_source());
        out.insert(out.end(), text.begin(), text.end());
        return;
    }
    if (const auto* const e = get_if<ast::Escaped>(&c)) {
        out.push_back(e->get_char(context.get_source()));
        return;
    }
    if (const auto* const b = get_if<ast::Generated>(&c)) {
        if (b->get_type() == ast::Generated_Type::plaintext) {
            append(out, b->as_string());
        }
        return;
    }
    if (const auto* const d = get_if<ast::Directive>(&c)) {
        if (Directive_Behavior* const behavior = context.find_directive(*d)) {
            behavior->generate_plaintext(out, *d, context);
            return;
        }
        try_lookup_error(*d, context);
        if (Directive_Behavior* const eb = context.get_error_behavior()) {
            eb->generate_plaintext(out, *d, context);
        }
        return;
    }
    MMML_ASSERT_UNREACHABLE(u8"Invalid form of content.");
}

void to_plaintext(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Context& context
)
{
    for (const ast::Content& c : content) {
        to_plaintext(out, c, context);
    }
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
                MMML_ASSERT_UNREACHABLE(u8"Generated content during syntax highlighting?!");
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
    Context& context
)
{
    // TODO: to be accurate, we would have to process HTML entities here so that syntax highlighting
    //       sees them as a character rather than attempting to highlight the original entity.
    //       For example, `&lt;` should be highlighted like a `<` operator.
    const std::u8string_view text = t.get_text(context.get_source());
    out.insert(out.end(), text.begin(), text.end());

    const Source_Span pos = t.get_source_span();
    MMML_ASSERT(pos.length == text.length());

    const std::size_t initial_size = out_mapping.size();
    out_mapping.reserve(initial_size + pos.length);
    for (std::size_t i = pos.begin; i < pos.end(); ++i) {
        out_mapping.push_back(i);
    }
    MMML_ASSERT(out_mapping.size() - initial_size == text.size());
}

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char8_t>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Escaped& e,
    Context& context
)
{
    out.push_back(e.get_char(context.get_source()));
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
    // Mixed or pure HTML directives don't interoperate with syntax highlighting at all.
    // There's no way to highlighting something like a `<button>` element,
    // and even if our directive was meant to generate e.g. `Hello: <button>...`,
    // it is not reasonable to assume that `Hello: ` can be highlighted meaningfully.
    case Directive_Category::mixed:
    case Directive_Category::pure_html: break;

    // Formatting directives such as `\b` are very special;
    // it is guaranteed that we can apply syntax highlighting to the content within,
    // and feed that back into the formatting directive.
    //
    // In this function, we just recurse into the directive's contents so we know which piece of
    // content within produced what syntax-highlighted part.
    case Directive_Category::formatting:
        to_plaintext_mapped_for_highlighting(out, out_mapping, d.get_content(), context);
        break;

    // For pure plaintext directives, we just run plaintext generation.
    // This also means that we don't know exactly which generated character belongs to
    // which source character, but it doesn't really matter.
    // We never run HTML generation afterwards and substitute the plaintext directive
    // with various syntax-highlighted content.
    case Directive_Category::pure_plaintext:
        const std::size_t initial_out_size = out.size();
        const std::size_t initial_mapping_size = out_mapping.size();
        behavior->generate_plaintext(out, d, context);
        MMML_ASSERT(out.size() >= initial_out_size);
        const std::size_t out_growth = out.size() - initial_out_size;
        out_mapping.reserve(out_mapping.size() + out_growth);
        const std::size_t d_begin = d.get_source_span().begin;
        for (std::size_t i = initial_out_size; i < out.size(); ++i) {
            out_mapping.push_back(d_begin);
        }
        const std::size_t mapping_growth = out_mapping.size() - initial_mapping_size;
        MMML_ASSERT(out_growth == mapping_growth);
        break;
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

void to_html(HTML_Writer& out, const ast::Text& text, Context& context)
{
    const std::u8string_view output = text.get_text(context.get_source());
    out.write_inner_text(output);
}

void to_html(HTML_Writer& out, const ast::Escaped& escaped, Context& context)
{
    const char8_t c = escaped.get_char(context.get_source());
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
    if (Directive_Behavior* const eb = context.get_error_behavior()) {
        eb->generate_html(out, directive, context);
    }
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
    for (std::size_t i = 0; i < content.size(); ++i) {
        if (const auto* const text = std::get_if<ast::Text>(&content[i])) {
            std::u8string_view str = text->get_text(context.get_source());
            // Note that the following two conditions are not mutually exclusive
            // when content contains just one element.
            if (i == 0) {
                str = trim_ascii_blank_left(str);
            }
            if (i + 1 == content.size()) {
                str = trim_ascii_blank_right(str);
            }
            // Other trimming mechanisms should have eliminated completely blank strings.
            MMML_ASSERT(!str.empty());
            out.write_inner_html(str);
        }
        else {
            to_html(out, content[i], context);
        }
    }
}

struct To_HTML_Paragraphs {
private:
    HTML_Writer& m_out;
    Context& m_context;
    bool m_in_paragraph = false;

public:
    To_HTML_Paragraphs(HTML_Writer& out, Context& context)
        : m_out { out }
        , m_context { context }
    {
    }

    //  Some directives split paragraphs, and some are inline.
    //  For example, `\\b{...}` gets displayed inline,
    //  but `\\blockquote` is block content.
    void operator()(const ast::Directive& d)
    {
        if (Directive_Behavior* const behavior = m_context.find_directive(d)) {
            on_directive(*behavior, d);
        }
        try_lookup_error(d, m_context);
        if (Directive_Behavior* const eb = m_context.get_error_behavior()) {
            on_directive(*eb, d);
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
        std::u8string_view text = t.get_text(m_context.get_source());
        if (trim_left) {
            text = trim_ascii_blank_left(text);
        }
        if (trim_right) {
            text = trim_ascii_blank_left(text);
        }

        while (!text.empty()) {
            if (const Blank_Line blank = find_blank_line_sequence(text)) {
                if (blank.begin != 0) {
                    transition(Directive_Display::in_line);
                    m_out.write_inner_text(text.substr(0, blank.begin));
                    text.remove_prefix(blank.begin);
                }
                transition(Directive_Display::block);
                text.remove_prefix(blank.length);
            }
            else {
                MMML_ASSERT(blank.begin == 0);
                transition(Directive_Display::in_line);
                m_out.write_inner_text(text);
                break;
            }
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
        case Directive_Display::none: return;

        case Directive_Display::in_line:
            if (!m_in_paragraph && display == Directive_Display::in_line) {
                m_out.open_tag(u8"p");
                m_out.write_inner_html(u8'\n');
                m_in_paragraph = true;
            }
            return;

        case Directive_Display::block:
            if (m_in_paragraph && display == Directive_Display::block) {
                m_out.close_tag(u8"p");
                m_out.write_inner_html(u8'\n');
                m_in_paragraph = false;
            }
            return;
        }
        MMML_ASSERT_UNREACHABLE(u8"Invalid display value.");
    }

    void on_directive(Directive_Behavior& b, const ast::Directive& d)
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
    To_HTML_Mode mode
)
{
    if (to_html_mode_is_trimmed(mode)) {
        content = trim_blank_text(content, context);
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
        To_HTML_Paragraphs impl { out, context };

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

void to_html_literally(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
{
    for (const ast::Content& c : content) {
        if (const auto* const e = get_if<ast::Escaped>(&c)) {
            const char8_t c = e->get_char(context.get_source());
            out.write_inner_html(c);
        }
        if (const auto* const t = get_if<ast::Text>(&c)) {
            out.write_inner_html(t->get_text(context.get_source()));
        }
        if (const auto* const _ = get_if<ast::Generated>(&c)) {
            MMML_ASSERT_UNREACHABLE(u8"Attempting to generate literal HTML from Behaved_Content");
            return;
        }
        if (const auto* const d = get_if<ast::Directive>(&c)) {
            out.write_inner_text(d->get_source(context.get_source()));
        }
    }
}

namespace {

constexpr std::u8string_view highlighting_tag = u8"h-";

struct Highlighted_Content_Behavior final : Content_Behavior {
    const std::u8string_view m_classes;

    [[nodiscard]]
    Highlighted_Content_Behavior(std::u8string_view classes)
        : Content_Behavior(Directive_Category::formatting, Directive_Display::in_line)
        , m_classes { classes }
    {
    }

    void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        std::span<const ast::Content> content,
        Context& context
    ) const final
    {
        to_plaintext(out, content, context);
    }

    void generate_html(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
        const final
    {
        out.open_tag_with_attributes(highlighting_tag)
            .write_attribute(u8"class", m_classes, Attribute_Style::always_double)
            .end();
        to_html(out, content, context);
        out.close_tag(highlighting_tag);
    }
};

std::pmr::vector<ast::Content> copy_highlighted(
    std::span<const ast::Content> content,
    std::u8string_view highlighted_source,
    std::span<const std::size_t> to_source_index,
    std::span<const HLJS_Annotation_Span*> to_highlight_span,
    Context& context
);

struct [[nodiscard]] Highlighted_AST_Copier {
    std::pmr::vector<ast::Content>& out;

    std::u8string_view source;
    std::span<const std::size_t> to_source_index;
    std::span<const HLJS_Annotation_Span*> to_span;
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
        MMML_ASSERT_UNREACHABLE(u8"Generated content during highlighting?");
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
        case Directive_Category::mixed:
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
                                                  .to_source_index = to_source_index,
                                                  .to_span = to_span,
                                                  .context = context,
                                                  .index = index };
            for (const auto& c : directive.get_content()) {
                std::visit(inner_copier, c);
            }
            MMML_ASSERT(inner_copier.index >= index);
            index = inner_copier.index;

            std::pmr::vector<ast::Argument> copied_arguments { directive.get_arguments().begin(),
                                                               directive.get_arguments().end(),
                                                               context.get_transient_memory() };

            out.push_back(ast::Directive { directive.get_source_span(), directive.get_name_length(),
                                           std::move(copied_arguments), std::move(inner_content) });
            return;
        }
        }
    }

private:
    void append_highlighted_text_in(Source_Span source_span)
    {
        const std::size_t limit = to_source_index.size();
        while (index < limit) {
            if (to_source_index[index] < source_span.begin) {
                ++index;
                continue;
            }
            if (to_source_index[index] >= source_span.end()) {
                break;
            }
            const HLJS_Annotation_Span* const current_span = to_span[index];
            const std::size_t snippet_begin = index++;
            for (; index < limit && to_source_index[index] < source_span.end(); ++index) {
                if (to_span[index] != current_span) {
                    break;
                }
            };
            out.push_back(
                make_generated(source.substr(snippet_begin, index - snippet_begin), current_span)
            );
        }
    }

    [[nodiscard]]
    ast::Generated
    make_generated(std::u8string_view inner_text, const HLJS_Annotation_Span* span) const
    {
        std::pmr::vector<char8_t> span_data { context.get_transient_memory() };
        HTML_Writer span_writer { span_data };

        if (span) {
            span_writer.open_tag_with_attributes(highlighting_tag)
                .write_attribute(
                    u8"class", hljs_scope_css_classes(span->value), Attribute_Style::always_double
                )
                .end();
        }
        span_writer.write_inner_text(inner_text);
        if (span) {
            span_writer.close_tag(highlighting_tag);
        }

        return ast::Generated { std::move(span_data), ast::Generated_Type::html,
                                Directive_Display::in_line };
    }
};

/// @brief Creates a copy of the given `content`
// using the specified syntax highlighting information.
/// @param content The content to copy.
/// @param highlighted_source The highlighted source code.
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
    std::u8string_view highlighted_source,
    std::span<const std::size_t> to_source_index,
    std::span<const HLJS_Annotation_Span*> to_highlight_span,
    Context& context
)
{
    MMML_ASSERT(to_source_index.size() == highlighted_source.size());
    MMML_ASSERT(to_highlight_span.size() == highlighted_source.size());

    std::pmr::vector<ast::Content> result { context.get_transient_memory() };
    result.reserve(content.size());

    Highlighted_AST_Copier copier { .out = result,
                                    .source = highlighted_source,
                                    .to_source_index = to_source_index,
                                    .to_span = to_highlight_span,
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
    To_HTML_Mode mode
)
{
    MMML_ASSERT(!to_html_mode_is_paragraphed(mode));

    std::pmr::vector<char8_t> plaintext { context.get_transient_memory() };
    std::pmr::vector<std::size_t> plaintext_to_source_index { context.get_transient_memory() };
    to_plaintext_mapped_for_highlighting(plaintext, plaintext_to_source_index, content, context);
    MMML_ASSERT(plaintext.size() == plaintext_to_source_index.size());

    std::pmr::vector<HLJS_Annotation_Span> spans { context.get_transient_memory() };
    const std::u8string_view plaintext_str { plaintext.data(), plaintext.size() };

    const Syntax_Highlighter& highlighter = context.get_highlighter();
    const Result<void, Syntax_Highlight_Error> result = highlighter(spans, plaintext_str, language);
    if (!result) {
        return result.error();
    }

    std::pmr::vector<const HLJS_Annotation_Span*> plaintext_to_span { context.get_transient_memory(
    ) };
    plaintext_to_span.resize(plaintext.size());
    for (const HLJS_Annotation_Span& span : spans) {
        for (std::size_t i = span.begin; i < span.end(); ++i) {
            plaintext_to_span[i] = &span;
        }
    }

    std::pmr::vector<ast::Content> highlighted_content = copy_highlighted(
        content, plaintext_str, plaintext_to_source_index, plaintext_to_span, context
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

void arguments_to_attributes(Attribute_Writer& out, const ast::Directive& d, Context& context)
{
    constexpr auto style = Attribute_Style::double_if_needed;

    std::pmr::vector<char8_t> value { context.get_transient_memory() };
    for (const ast::Argument& a : d.get_arguments()) {
        // TODO: error handling
        value.clear();
        to_plaintext(value, a.get_content(), context);
        const std::u8string_view value_string { value.data(), value.size() };
        if (a.has_name()) {
            out.write_attribute(a.get_name(context.get_source()), value_string, style);
        }
        // TODO: what if the positional argument cannot be used as an attribute name
        else {
            out.write_empty_attribute(value_string, style);
        }
    }
}

} // namespace mmml

#include <ranges>
#include <span>
#include <vector>

#include "mmml/util/html_writer.hpp"

#include "mmml/context.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/directives.hpp"

namespace mmml {

Directive_Behavior* Context::find_directive(std::u8string_view name) const
{
    for (const Name_Resolver* const resolver : std::views::reverse(m_name_resolvers)) {
        if (Directive_Behavior* result = (*resolver)(name)) {
            return result;
        }
    }
    return nullptr;
}

Directive_Behavior* Context::find_directive(const ast::Directive& directive) const
{
    return find_directive(directive.get_name(m_source));
}

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
    if (const auto* const b = get_if<ast::Behaved_Content>(&c)) {
        b->get_behavior().generate_plaintext(out, b->get_content(), context);
        return;
    }
    if (const auto* const d = get_if<ast::Directive>(&c)) {
        if (Directive_Behavior* behavior = context.find_directive(*d)) {
            behavior->generate_plaintext(out, *d, context);
        }
        else if (Directive_Behavior* eb = context.get_error_behavior()) {
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
    if (const auto* const t = get_if<ast::Text>(&c)) {
        to_plaintext_mapped_for_highlighting(out, out_mapping, *t, context);
        return;
    }
    if (const auto* const e = get_if<ast::Escaped>(&c)) {
        out.push_back(e->get_char(context.get_source()));
        out_mapping.push_back(e->get_char_index());
        return;
    }
    if (const auto* const b = get_if<ast::Behaved_Content>(&c)) {
        MMML_ASSERT_UNREACHABLE(u8"Behaved content during syntax highlighting?!");
        b->get_behavior().generate_plaintext(out, b->get_content(), context);
        return;
    }
    if (const auto* const d = get_if<ast::Directive>(&c)) {
        to_plaintext_mapped_for_highlighting(out, out_mapping, *d, context);
        return;
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
    out_mapping.reserve(out_mapping.size() + pos.length);
    for (std::size_t i = pos.begin; i < pos.length; ++i) {
        out_mapping.push_back(i);
    }
}

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char8_t>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Directive& d,
    Context& context
)
{
    Directive_Behavior* behavior = context.find_directive(d);
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
        const std::size_t initial_size = out.size();
        behavior->generate_plaintext(out, d, context);
        MMML_ASSERT(out_mapping.size() >= initial_size);
        out_mapping.reserve(out_mapping.size() - initial_size);
        for (std::size_t i = initial_size; i < out.size(); ++i) {
            out_mapping.push_back(i);
        }
        break;
    }
}

void to_html_literally(HTML_Writer& out, std::span<const ast::Content> content, Context& context)
{
    for (const ast::Content& c : content) {
        if (const auto* const e = get_if<ast::Escaped>(&c)) {
            const char8_t c = e->get_char(context.get_source());
            out.write_inner_html({ &c, 1 });
        }
        if (const auto* const t = get_if<ast::Text>(&c)) {
            out.write_inner_html(t->get_text(context.get_source()));
        }
        if (const auto* const b = get_if<ast::Behaved_Content>(&c)) {
            MMML_ASSERT_UNREACHABLE(u8"Attempting to generate literal HTML from Behaved_Content");
            return;
        }
        if (const auto* const d = get_if<ast::Directive>(&c)) {
            out.write_inner_text(d->get_source(context.get_source()));
        }
    }
}

void preprocess(ast::Content& c, Context& context)
{
    if (auto* const b = get_if<ast::Behaved_Content>(&c)) {
        b->get_behavior().preprocess(b->get_content(), context);
        return;
    }
    if (auto* const d = get_if<ast::Directive>(&c)) {
        if (Directive_Behavior* behavior = context.find_directive(*d)) {
            behavior->preprocess(*d, context);
        }
        else if (Directive_Behavior* eb = context.get_error_behavior()) {
            behavior->preprocess(*d, context);
        }
        return;
    }
}

void preprocess(std::span<ast::Content> contents, Context& context)
{
    for (ast::Content& c : contents) {
        preprocess(c, context);
    }
}

void preprocess_arguments(ast::Directive& d, Context& context)
{
    for (ast::Argument& a : d.get_arguments()) {
        preprocess(a.get_content(), context);
    }
}

void arguments_to_attributes(Attribute_Writer& out, const ast::Directive& d, Context& context)
{
    std::pmr::vector<char8_t> value { context.get_transient_memory() };
    for (const ast::Argument& a : d.get_arguments()) {
        // TODO: error handling
        value.clear();
        to_plaintext(value, a.get_content(), context);
        const std::u8string_view value_string { value.data(), value.size() };
        if (a.has_name()) {
            out.write_attribute(a.get_name(context.get_source()), value_string);
        }
        // TODO: what if the positional argument cannot be used as an attribute name
        else {
            out.write_attribute(value_string);
        }
    }
}

} // namespace mmml

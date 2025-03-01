#ifndef MMML_DIRECTIVE_PROCESSING_HPP
#define MMML_DIRECTIVE_PROCESSING_HPP

#include <vector>

#include "mmml/ast.hpp"
#include "mmml/directives.hpp"

namespace mmml {

/// @brief Converts content to plaintext.
/// For text, this outputs that text literally.
/// For escaped characters, this outputs the escaped character.
/// For directives, this runs `generate_plaintext` using the behavior of that directive,
/// looked up via context.
/// @param context the current processing context
void to_plaintext(std::pmr::vector<char>& out, const ast::Content& c, Context& context);

void to_plaintext(
    std::pmr::vector<char>& out,
    std::span<const ast::Content> content,
    Context& context
);

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Content& c,
    Context& context
);

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Text& t,
    Context& context
);

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    const ast::Directive& d,
    Context& context
);

void to_plaintext_mapped_for_highlighting(
    std::pmr::vector<char>& out,
    std::pmr::vector<std::size_t>& out_mapping,
    std::span<const ast::Content> content,
    Context& context
);

void contents_to_html(
    Annotated_String& out,
    std::span<const ast::Content> content,
    Context& context
);

void preprocess_content(ast::Content& c, Context& context);

void preprocess_contents(std::span<ast::Content> contents, Context& context);

void preprocess_arguments(ast::Directive& d, Context& context);

void arguments_to_attributes(Attribute_Writer& out, const ast::Directive& d, Context& context);

} // namespace mmml

#endif

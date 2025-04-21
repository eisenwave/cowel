#include <memory_resource>
#include <unordered_set>

#include "mmml/diagnostic.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/html_writer.hpp"

#include "mmml/content_behavior.hpp"
#include "mmml/context.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/document_content_behavior.hpp"
#include "mmml/document_generation.hpp"
#include "mmml/document_sections.hpp"

namespace mmml {

void generate_document(const Generation_Options& options)
{
    MMML_ASSERT(options.memory != nullptr);

    std::pmr::unsynchronized_pool_resource transient_memory { options.memory };

    HTML_Writer writer { options.output };

    Context context { options.path,   options.source,      options.error_behavior, //
                      options.logger, options.highlighter, options.document_finder, //
                      options.memory, &transient_memory };
    context.add_resolver(options.builtin_behavior);

    options.root_behavior.generate_html(writer, options.root_content, context);
}

namespace {

// See also `reference_section`.
// The point of all this is that references to other sections are encoded using a section name,
// which is encoded using a Supplementary Private Use Area A code point
// storing the section name length, followed by the section name code units.

constexpr char8_t supplementary_pua_a_first_code_unit
    = utf8::encode8_unchecked(supplementary_pua_a_min).code_units[0];
constexpr char8_t supplementary_pua_a_first_code_unit_mask = 0b1111'1000;
constexpr char8_t supplementary_pua_a_first_code_unit_masked
    = supplementary_pua_a_first_code_unit & supplementary_pua_a_first_code_unit_mask;

static_assert(supplementary_pua_a_first_code_unit_masked == 0b1111'0000);

struct Reference_Resolver {
    std::pmr::vector<char8_t>& out;
    std::pmr::unordered_set<const void*>& visited;
    Context& context;

    bool operator()(std::u8string_view text);
};

bool Reference_Resolver::operator()(std::u8string_view text)
{
    bool success = true;

    std::size_t plain_length = 0;
    const auto flush = [&] {
        if (plain_length != 0) {
            out.insert(out.end(), text.data(), text.data() + plain_length);
            text.remove_prefix(plain_length);
            plain_length = 0;
        }
    };
    while (plain_length < text.length()) {
        // Obviously, almost none of the text in the generated document is a section reference,
        // so we perform this early test,
        // which tells us that it couldn't be a PUA A character based on the leading code unit.
        if ((text[plain_length] & supplementary_pua_a_first_code_unit_mask)
            != supplementary_pua_a_first_code_unit_masked) {
            ++plain_length;
            continue;
        }
        const auto [code_point, code_units]
            = utf8::decode_and_length_or_throw(text.substr(plain_length));
        // This test passes for any code point that is encoded as four UTF-8 code units,
        // so we still need to check if the code point is PUA A.
        if (code_point < supplementary_pua_a_min || code_point > supplementary_pua_a_max) {
            ++plain_length;
            continue;
        }
        MMML_DEBUG_ASSERT(code_units == 4);
        flush();
        const auto reference_length = std::size_t(code_point - supplementary_pua_a_min);
        MMML_ASSERT(4 + reference_length <= text.length());
        const std::u8string_view section_name = text.substr(4, reference_length);

        const Document_Sections::entry_type* const entry
            = context.get_sections().find(section_name);
        if (!entry) {
            if (context.emits(Severity::error)) {
                Diagnostic error = context.make_error(diagnostic::section_ref_not_found, {});
                error.message += u8"Invalid reference to section \"";
                error.message += section_name;
                error.message += u8"\".";
                context.emit(std::move(error));
            }
            success = false;
            continue;
        }
        const auto [_, insert_success] = visited.insert(entry);
        if (!insert_success) {
            if (context.emits(Severity::error)) {
                Diagnostic error = context.make_error(diagnostic::section_ref_circular, {});
                error.message += u8"Circular dependency in reference to section \"";
                error.message += section_name;
                error.message += u8"\".";
                context.emit(std::move(error));
            }
            success = false;
            continue;
        }

        text.remove_prefix(4 + reference_length);
        const std::u8string_view referenced_text { entry->second.data(), entry->second.size() };
        (*this)(referenced_text);
    }
    flush();
    return success;
}

} // namespace

void Head_Body_Content_Behavior::generate_html(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    Context& context
) const
{
    Document_Sections& sections = context.get_sections();

    const Document_Sections::entry_type* html_section = nullptr;
    auto& html_text = [&] -> std::pmr::vector<char8_t>& {
        const auto scope = sections.go_to_scoped(u8"html");

        HTML_Writer current_out = sections.current_html();
        const auto open_and_close = [&](std::u8string_view tag, auto f) {
            current_out.open_tag(tag);
            current_out.write_inner_html(u8'\n');
            f();
            current_out.close_tag(tag);
            current_out.write_inner_html(u8'\n');
        };

        open_and_close(u8"html", [&] {
            open_and_close(u8"head", [&] { reference_section(current_out, u8"head"); });
            open_and_close(u8"body", [&] { reference_section(current_out, u8"body"); });
        });

        html_section = &sections.current();
        return sections.current_text();
    }();
    const std::u8string_view html_string { html_text.data(), html_text.size() };

    {
        const auto scope = sections.go_to_scoped(u8"head");
        HTML_Writer current_out = sections.current_html();
        generate_head(current_out, content, context);
    }
    {
        const auto scope = sections.go_to_scoped(u8"body");
        HTML_Writer current_out = sections.current_html();
        generate_body(current_out, content, context);
    }

    std::pmr::unordered_set<const void*> visited(context.get_transient_memory());
    visited.insert(html_section);

    Reference_Resolver { out.get_output(), visited, context }(html_string);
}

void Document_Content_Behavior::generate_head(HTML_Writer&, std::span<const ast::Content>, Context&)
    const
{
    // TODO: implement
}

void Document_Content_Behavior::generate_body(
    HTML_Writer& out,
    std::span<const ast::Content> content,
    Context& context
) const
{
    to_html(out, content, context);
}

} // namespace mmml

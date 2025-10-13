#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/unicode.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/paragraph_split.hpp"

#include "cowel/assets.hpp"
#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_generation.hpp"
#include "cowel/document_sections.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"
#include "cowel/theme_to_css.hpp"

using namespace std::string_view_literals;

namespace cowel {

Processing_Status run_generation(
    Function_Ref<Processing_Status(Context&)> generate,
    const Generation_Options& options
)
{
    COWEL_ASSERT(generate);
    COWEL_ASSERT(options.memory != nullptr);

    std::pmr::unsynchronized_pool_resource transient_memory { options.memory };

    Context context {
        options.highlight_theme_source,
        options.error_behavior,
        options.builtin_name_resolver,
        options.file_loader,
        options.logger,
        options.highlighter,
        options.memory,
        &transient_memory,
    };
    return generate(context);
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
    Text_Sink& out;
    std::pmr::unordered_set<const Document_Sections::entry_type*>& visited;
    Context& context;

    bool operator()(std::u8string_view text, File_Id file);
};

bool Reference_Resolver::operator()(std::u8string_view text, File_Id file)
{
    bool success = true;

    std::size_t plain_length = 0;
    const auto flush = [&] {
        if (plain_length != 0) {
            out.write(text.substr(0, plain_length), Output_Language::html);
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
            = utf8::decode_and_length_or_replacement(text.substr(plain_length));
        // This test passes for any code point that is encoded as four UTF-8 code units,
        // so we still need to check if the code point is PUA A.
        if (code_point < supplementary_pua_a_min || code_point > supplementary_pua_a_max) {
            ++plain_length;
            continue;
        }
        COWEL_DEBUG_ASSERT(code_units == 4);
        flush();
        const auto reference_length = std::size_t(code_point - supplementary_pua_a_min);
        COWEL_ASSERT(4 + reference_length <= text.length());
        const std::u8string_view section_name = text.substr(4, reference_length);

        bool section_success = true;
        const Document_Sections::entry_type* const entry
            = context.get_sections().find(section_name);
        if (!entry) {
            const std::u8string_view message[] {
                u8"Invalid reference to section \"",
                section_name,
                u8"\".",
            };
            context.try_error(
                diagnostic::section_ref_not_found, { {}, file }, joined_char_sequence(message)
            );
            section_success = false;
        }
        else if (const auto [_, insert_success] = visited.insert(entry); !insert_success) {
            const std::u8string_view message[] {
                u8"Circular dependency in reference to section \"",
                section_name,
                u8"\".",
            };
            context.try_error(
                diagnostic::section_ref_circular, { {}, file }, joined_char_sequence(message)
            );
            section_success = false;
        }
        text.remove_prefix(4 + reference_length);
        if (section_success) {
            const std::u8string_view referenced_text = entry->second.text();
            (*this)(referenced_text, file);
        }
        else {
            success = section_success;
        }
        visited.erase(entry);
    }
    flush();
    return success;
}

} // namespace

Processing_Status write_head_body_document(
    Text_Sink& out,
    std::span<const ast::Content> content,
    Context& context,
    Function_Ref<Processing_Status(Content_Policy&, std::span<const ast::Content>, Context&)> head,
    Function_Ref<Processing_Status(Content_Policy&, std::span<const ast::Content>, Context&)> body
)
{
    Document_Sections& sections = context.get_sections();

    const Document_Sections::entry_type* html_section = nullptr;
    std::u8string_view html_string = [&] -> std::u8string_view {
        const auto scope = sections.go_to_scoped(section_name::document_html);

        HTML_Writer_Buffer buffer { sections.current_policy(), Output_Language::html };
        Text_Buffer_HTML_Writer current_writer { buffer };

        const auto open_and_close = [&](HTML_Tag_Name tag, auto f) {
            current_writer.open_tag(tag);
            current_writer.write_inner_html(u8'\n');
            f();
            current_writer.close_tag(tag);
            current_writer.write_inner_html(u8'\n');
        };

        current_writer.write_preamble();
        open_and_close(html_tag::html, [&] {
            open_and_close(html_tag::head, [&] {
                reference_section(buffer, section_name::document_head);
            });
            open_and_close(html_tag::body, [&] {
                reference_section(buffer, section_name::document_body);
            });
        });

        html_section = &sections.current();
        buffer.flush();
        return as_u8string_view(sections.current_output());
    }();

    auto status = Processing_Status::ok;
    {
        const auto scope = sections.go_to_scoped(section_name::document_head);
        status = status_concat(status, head(sections.current_policy(), content, context));
        if (status_is_break(status)) {
            return status;
        }
    }
    {
        const auto scope = sections.go_to_scoped(section_name::document_body);
        status = status_concat(status, body(sections.current_policy(), content, context));
        if (status_is_break(status)) {
            return status;
        }
    }

    const auto file = content.empty() ? File_Id::main : content.front().get_source_span().file;
    const bool res_success = resolve_references(out, html_string, context, file);
    const auto res_status = res_success ? Processing_Status::ok : Processing_Status::error;
    status = status_concat(status, res_status);

    return status;
}

bool resolve_references(Text_Sink& out, std::u8string_view text, Context& context, File_Id file)
{
    std::pmr::unordered_set<const Document_Sections::entry_type*> visited(
        context.get_transient_memory()
    );
    visited.insert(&context.get_sections().current());

    return Reference_Resolver { out, visited, context }(text, file);
}

constexpr std::u8string_view indent = u8"  ";
constexpr std::u8string_view newline_indent = u8"\n  ";

Processing_Status
write_wg21_head_contents(Content_Policy& out, std::span<const ast::Content>, Context& context)
{
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    constexpr std::u8string_view google_fonts_url
        = u8"https://fonts.googleapis.com/css2"
          u8"?family=Fira+Code:wght@300..700"
          u8"&family=Noto+Sans:ital,wght@0,100..900;1,100..900"
          u8"&family=Noto+Serif:ital,wght@0,100..900;1,100..900"
          u8"&display=swap";

    writer.write_inner_html(indent);

    writer
        .open_tag_with_attributes(html_tag::meta) //
        .write_charset(u8"UTF-8"sv)
        .end_empty();
    writer
        .open_tag_with_attributes(html_tag::meta) //
        .write_name(u8"viewport"sv)
        .write_content(u8"width=device-width, initial-scale=1"sv)
        .end_empty();

    writer
        .open_tag_with_attributes(html_tag::link) //
        .write_rel(u8"preconnect"sv)
        .write_href(u8"https://fonts.googleapis.com"sv)
        .end_empty();
    writer.write_inner_html(newline_indent);
    writer
        .open_tag_with_attributes(html_tag::link) //
        .write_rel(u8"preconnect"sv)
        .write_href(u8"https://fonts.gstatic.com"sv)
        .write_crossorigin()
        .end_empty();
    writer.write_inner_html(newline_indent);
    writer
        .open_tag_with_attributes(html_tag::link) //
        .write_rel(u8"stylesheet"sv)
        .write_href(google_fonts_url)
        .end_empty();

    const auto include_css_or_js = [&](HTML_Tag_Name tag, std::u8string_view source) {
        writer.write_inner_html(newline_indent);
        writer.open_tag(tag);
        writer.write_inner_html(u8'\n');
        writer.write_inner_html(source);
        writer.write_inner_html(indent);
        writer.close_tag(tag);
    };
    include_css_or_js(html_tag::style, assets::main_css);
    {
        writer.write_inner_text(newline_indent);
        writer.open_tag(html_tag::style);
        writer.write_inner_html(u8'\n');
        const std::u8string_view theme_json = context.get_highlight_theme_source();
        std::pmr::vector<char8_t> css { context.get_transient_memory() };
        if (theme_to_css(css, theme_json, context.get_transient_memory())) {
            writer.write_inner_html(as_u8string_view(css));
        }
        else {
            context.try_error(
                diagnostic::theme_conversion, { {}, File_Id::main },
                u8"Failed to convert the syntax highlight theme to CSS, "
                u8"possibly because the JSON was malformed."sv
            );
            return Processing_Status::error;
        }
        writer.write_inner_html(indent);
        writer.close_tag(html_tag::style);
    }

    include_css_or_js(html_tag::script, assets::light_dark_js);
    writer.write_inner_html(u8'\n');
    buffer.flush();
    return Processing_Status::ok;
}

Processing_Status write_wg21_body_contents(
    Content_Policy& out,
    std::span<const ast::Content> content,
    Context& context
)
{
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    writer.write_inner_html(assets::settings_widget_html);
    writer.open_tag(html_tag::main);
    writer.write_inner_html(u8'\n');

    Paragraph_Split_Policy policy { buffer, context.get_transient_memory() };
    const Processing_Status result = consume_all(policy, content, Frame_Index::root, context);
    policy.leave_paragraph();

    writer.close_tag(html_tag::main);
    return result;
}

} // namespace cowel

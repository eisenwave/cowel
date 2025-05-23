#include <algorithm>
#include <string_view>
#include <vector>

#include "cowel/diagnostic.hpp"
#include "cowel/util/chars.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_sections.hpp"
#include "cowel/util/to_chars.hpp"

namespace cowel {
namespace {

void trim_left(std::pmr::vector<char8_t>& text)
{
    const std::size_t amount = length_blank_left(as_u8string_view(text));
    COWEL_ASSERT(amount <= text.size());
    text.erase(text.begin(), text.begin() + std::ptrdiff_t(amount));
}

void trim_right(std::pmr::vector<char8_t>& text)
{
    const std::size_t amount = length_blank_right(as_u8string_view(text));
    COWEL_ASSERT(amount <= text.size());
    text.erase(text.end() - std::ptrdiff_t(amount), text.end());
}

void trim(std::pmr::vector<char8_t>& text)
{
    trim_left(text);
    trim_right(text);
}

void sanitize_id(std::pmr::vector<char8_t>& id)
{
    trim(id);
    for (char8_t& c : id) {
        if (is_ascii_upper_alpha(c)) {
            c = to_ascii_lower(c);
        }
        else if (is_html_whitespace(c)) {
            c = u8'-';
        }
    }
}

bool synthesize_id(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Context& context
)
{
    // TODO: diagnostic on bad status
    const To_Plaintext_Status status
        = to_plaintext(out, content, context, To_Plaintext_Mode::no_side_effects);
    if (status == To_Plaintext_Status::error) {
        return false;
    }
    sanitize_id(out);
    return true;
}

// TODO: this should be done via variables and the context or something
thread_local int h_counters[6] {};

constexpr int min_listing_level = 2;
constexpr int max_listing_level = 6;

} // namespace

void Heading_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    static constexpr std::u8string_view parameters[] { u8"id", u8"listed" };

    const auto level_char = char8_t(int(u8'0') + m_level);
    const char8_t tag_name_data[2] { u8'h', level_char };
    const std::u8string_view tag_name { tag_name_data, sizeof(tag_name_data) };

    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments(), Parameter_Match_Mode::only_named);

    // Determine whether the heading should be listed in the table of contents.
    const auto is_listed = [&] -> bool {
        // h1 headings are not listed by default because it would be silly
        // for the top-level heading to re-appear in the table of contents.
        // Also, low-level headings like h5 and h6 are typically not relevant.
        bool listed_by_default = m_level >= min_listing_level && m_level <= max_listing_level;
        std::pmr::vector<char8_t> listed_data { context.get_transient_memory() };
        if (!argument_to_plaintext(listed_data, d, args, u8"listed", context)) {
            return listed_by_default;
        }
        const auto listed_string = as_u8string_view(listed_data);
        if (listed_string == u8"no") {
            return false;
        }
        if (listed_string == u8"yes") {
            return true;
        }
        // TODO: add invalid enum diagnostics
        return listed_by_default;
    }();

    if (is_listed) {
        // Update heading numbers.
        COWEL_ASSERT(m_level >= 1 && m_level <= 6);
        ++h_counters[m_level - 1];
        std::ranges::fill(h_counters + m_level, std::end(h_counters), 0);
    }

    std::pmr::vector<char8_t> id_data { context.get_transient_memory() };
    bool has_id = false;

    // 1. Obtain or synthesize the id.
    Attribute_Writer attributes = out.open_tag_with_attributes(tag_name);
    if (const int id_index = args.get_argument_index(u8"id"); id_index < 0) {
        if (synthesize_id(id_data, d.get_content(), context) && !id_data.empty()) {
            attributes.write_id(as_u8string_view(id_data));
            has_id = true;
        }
    }
    else {
        const ast::Argument& id_arg = d.get_arguments()[std::size_t(id_index)];
        const To_Plaintext_Status status = to_plaintext(id_data, id_arg.get_content(), context);
        if (status != To_Plaintext_Status::error) {
            attributes.write_id(as_u8string_view(id_data));
            has_id = !id_data.empty();
        }
    }
    arguments_to_attributes(attributes, d, context, [](std::u8string_view name) {
        return !std::ranges::contains(parameters, name);
    });
    attributes.end();

    // 2. Generate user content in the heading.
    std::pmr::vector<char8_t> heading_html { context.get_transient_memory() };
    HTML_Writer heading_html_writer { heading_html };
    to_html(heading_html_writer, d.get_content(), context);
    const auto heading_html_string = as_u8string_view(heading_html);

    // 3. Check for id duplication.
    const bool has_valid_id = [&] {
        if (!has_id) {
            return false;
        }
        const std::u8string_view id_string_view { id_data.data(), id_data.size() };
        std::pmr::u8string id_string { id_string_view, context.get_transient_memory() };
        if (context.emplace_id(std::move(id_string), { heading_html_string })) {
            return true;
        }
        const std::u8string_view message[] {
            u8"Duplicate id \"",
            id_string_view,
            u8"\". Heading will be generated, but references may be broken.",
        };
        context.try_warning(diagnostic::duplicate_id, d.get_source_span(), message);
        return false;
    }();

    // 4. Surround user content with paragraph/anchor link.
    if (has_valid_id) {
        id_data.insert(id_data.begin(), u8'#');
        out.open_tag_with_attributes(u8"a") //
            .write_class(u8"para")
            .write_href(as_u8string_view(id_data))
            .end();
        out.close_tag(u8"a");
    }
    const auto write_numbers = [&](HTML_Writer& to) {
        for (int i = min_listing_level; i <= m_level; ++i) {
            if (i != min_listing_level) {
                to.write_inner_html(u8'.');
            }
            const int counter = h_counters[i - 1];
            to.write_inner_html(to_characters8(counter).as_string());
        }
    };
    if (is_listed) {
        write_numbers(out);
        out.write_inner_html(u8". ");
    }
    out.write_inner_html(heading_html_string);
    out.close_tag(tag_name);

    // 6. Also write an ID preview in case the heading is referenced via \ref[#id]

    if (has_valid_id) {
        Document_Sections& sections = context.get_sections();
        std::pmr::u8string section_name { context.get_transient_memory() };
        section_name += section_name::id_preview;
        section_name += u8'.';
        COWEL_ASSERT(id_data.front() == u8'#');
        section_name += as_u8string_view(id_data).substr(1);

        const auto scope = sections.go_to_scoped(section_name);
        HTML_Writer id_preview_out = sections.current_html();
        id_preview_out.write_inner_html(u8"§");
        if (is_listed) {
            write_numbers(id_preview_out);
            id_preview_out.write_inner_html(u8". ");
        }
        else {
            id_preview_out.write_inner_html(u8' ');
        }
        id_preview_out.write_inner_html(heading_html_string);
    }

    // 7. If necessary, also output the heading into the table of contents.
    if (is_listed) {
        Document_Sections& sections = context.get_sections();
        const auto scope = sections.go_to_scoped(section_name::table_of_contents);
        HTML_Writer toc_writer = sections.current_html();

        toc_writer
            .open_tag_with_attributes(u8"div") //
            .write_class(u8"toc-num")
            .write_attribute(u8"data-level", tag_name.substr(1))
            .end();
        write_numbers(toc_writer);
        toc_writer.close_tag(u8"div");
        toc_writer.write_inner_html(u8'\n'); // non-functional, purely for prettier HTML output

        if (has_valid_id) {
            toc_writer
                .open_tag_with_attributes(u8"a") //
                .write_href(as_u8string_view(id_data))
                .end();
        }

        toc_writer.open_tag(tag_name);
        toc_writer.write_inner_html(heading_html_string);
        toc_writer.close_tag(tag_name);

        if (has_valid_id) {
            toc_writer.close_tag(u8"a");
        }
        toc_writer.write_inner_html(u8'\n'); // non-functional, purely for prettier HTML output
    }
}

namespace {

void generate_sectioned(
    const ast::Directive& d,
    Context& context,
    std::u8string_view no_section_diagnostic,
    Function_Ref<void(std::u8string_view section)> action
)
{
    static constexpr std::u8string_view parameters[] { u8"section" };
    Argument_Matcher args { parameters, context.get_persistent_memory() };
    args.match(d.get_arguments());

    const int arg_index = args.get_argument_index(u8"section");
    if (arg_index < 0) {
        context.try_error(no_section_diagnostic, d.get_source_span(), u8"No section was provided.");
        return;
    }

    std::pmr::vector<char8_t> name_data { context.get_transient_memory() };
    const ast::Argument& arg = d.get_arguments()[std::size_t(arg_index)];
    to_plaintext(name_data, arg.get_content(), context);
    const auto section_string = as_u8string_view(name_data);
    if (section_string.empty()) {
        context.try_error(no_section_diagnostic, d.get_source_span(), u8"No section was provided.");
        return;
    }

    action(section_string);
}

} // namespace

void There_Behavior::evaluate(const ast::Directive& d, Context& context) const
{
    generate_sectioned(d, context, diagnostic::there::no_section, [&](std::u8string_view section) {
        const auto scope = context.get_sections().go_to_scoped(section);
        HTML_Writer there_writer = context.get_sections().current_html();
        to_html(there_writer, d.get_content(), context);
    });
}

void Here_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    generate_sectioned(d, context, diagnostic::there::no_section, [&](std::u8string_view section) {
        reference_section(out, section);
    });
}

void Make_Section_Behavior::generate_html(HTML_Writer& out, const ast::Directive&, Context& context)
    const
{
    context.get_sections().make(m_section_name);

    // TODO: warn about ignored arguments and block
    out.open_tag_with_attributes(u8"div") //
        .write_class(m_class_name)
        .end();
    reference_section(out, m_section_name);
    out.close_tag(u8"div");
}

} // namespace cowel

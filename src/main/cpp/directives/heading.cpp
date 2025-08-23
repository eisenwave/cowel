#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/parameters.hpp"
#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/function_ref.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/to_chars.hpp"

#include "cowel/policy/capture.hpp"
#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_sections.hpp"
#include "cowel/invocation.hpp"
#include "cowel/output_language.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

[[nodiscard]]
Processing_Status synthesize_id(
    std::pmr::vector<char8_t>& out,
    std::span<const ast::Content> content,
    Frame_Index content_frame,
    Context& context
)
{
    // TODO: diagnostic on bad status
    // TODO: disallow side effects, or maybe use content policies to pull out just the text?
    const auto status = to_plaintext(out, content, content_frame, context);
    if (status != Processing_Status::ok) {
        return status;
    }
    sanitize_html_id(out);
    return Processing_Status::ok;
}

// TODO: this should be done via variables and the context or something
thread_local int h_counters[6] {};

constexpr int min_listing_level = 2;
constexpr int max_listing_level = 6;

} // namespace

Processing_Status
Heading_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    String_Matcher id_matcher { context.get_transient_memory() };
    Group_Member_Matcher id_member { u8"id"sv, Optionality::optional, id_matcher };
    Boolean_Matcher listed_boolean;
    Group_Member_Matcher listed_member { u8"listed"sv, Optionality::optional, listed_boolean };
    Boolean_Matcher show_number_boolean;
    Group_Member_Matcher show_number_member { u8"show-number"sv, Optionality::optional,
                                              show_number_boolean };
    Group_Pack_Named_Lazy_Any_Matcher attr_group;
    Group_Member_Matcher attr_member { u8"attr"sv, Optionality::optional, attr_group };
    Group_Member_Matcher* const parameters[] {
        &id_member,
        &listed_member,
        &show_number_member,
        &attr_member,
    };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    const auto level_char = char8_t(int(u8'0') + m_level);
    const char8_t tag_name_data[2] { u8'h', level_char };
    const HTML_Tag_Name tag_name { std::u8string_view { tag_name_data, sizeof(tag_name_data) } };
    const bool listed_by_default = m_level >= min_listing_level && m_level <= max_listing_level;

    const bool is_listed = listed_boolean.get_or_default(listed_by_default);
    const bool is_number_shown = show_number_boolean.get_or_default(listed_by_default);

    if (is_listed) {
        // Update heading numbers.
        COWEL_ASSERT(m_level >= 1 && m_level <= 6);
        ++h_counters[m_level - 1];
        std::ranges::fill(h_counters + m_level, std::end(h_counters), 0);
    }

    struct Id {
        Processing_Status status;
        std::u8string_view string;
        bool exists;
    };

    std::pmr::vector<char8_t> synthesized_id_buffer { context.get_transient_memory() };
    const auto id = [&] -> Id {
        if (id_matcher.was_matched()) {
            return {
                .status = Processing_Status::ok,
                .string = id_matcher.get(),
                .exists = true,
            };
        }

        const auto status = synthesize_id(
            synthesized_id_buffer, call.get_content_span(), call.content_frame, context
        );
        return {
            .status = status,
            .string = as_u8string_view(synthesized_id_buffer),
            .exists = !synthesized_id_buffer.empty(),
        };
    }();
    if (status_is_break(id.status)) {
        return id.status;
    }
    Processing_Status current_status = id.status;

    // 0. Ensure that headings are not in paragraphs.
    try_leave_paragraph(out);

    // 1. Write HTML attributes, including the (possibly synthesized) id.
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(tag_name);
    if (id.exists) {
        attributes.write_id(id.string);
    }
    const auto attributes_status = attr_group.was_matched()
        ? named_arguments_to_attributes(
              attributes, attr_group.get().get_members(), attr_group.get_frame(), context
          )
        : Processing_Status::ok;
    attributes.end();
    current_status = status_concat(current_status, attributes_status);
    if (status_is_break(attributes_status)) {
        writer.close_tag(tag_name);
        buffer.flush();
        return current_status;
    }

    // 2. Generate user content in the heading.
    std::pmr::vector<char8_t> heading_html { context.get_transient_memory() };
    {
        Capturing_Ref_Text_Sink heading_sink { heading_html, Output_Language::html };
        HTML_Content_Policy html_policy { heading_sink };
        const auto heading_status
            = consume_all(html_policy, call.get_content_span(), call.content_frame, context);
        current_status = status_concat(current_status, heading_status);
        if (status_is_break(heading_status)) {
            writer.close_tag(tag_name);
            buffer.flush();
            return current_status;
        }
    }
    const auto heading_html_string = as_u8string_view(heading_html);

    // 3. Check for id duplication.
    const bool has_valid_id = [&] {
        if (!id.exists) {
            return false;
        }
        std::pmr::u8string persistent_id_string { id.string, context.get_transient_memory() };
        if (context.emplace_id(std::move(persistent_id_string), { heading_html_string })) {
            return true;
        }
        context.try_warning(
            diagnostic::duplicate_id, call.directive.get_source_span(),
            joined_char_sequence({
                u8"Duplicate id \"",
                id.string,
                u8"\". Heading will be generated, but references may be broken.",
            })
        );
        return false;
    }();

    // 4. Surround user content with paragraph/anchor link.
    if (has_valid_id) {
        writer
            .open_tag_with_attributes(html_tag::a) //
            .write_class(u8"para"sv)
            .write_url_attribute(html_attr::href, joined_char_sequence({ u8"#"sv, id.string }))
            .end();
        writer.close_tag(html_tag::a);
    }
    const auto write_numbers = [&](Text_Buffer_HTML_Writer& to) {
        for (int i = min_listing_level; i <= m_level; ++i) {
            if (i != min_listing_level) {
                to.write_inner_html(u8'.');
            }
            const int counter = h_counters[i - 1];
            to.write_inner_html(to_characters8(counter).as_string());
        }
    };
    if (is_listed && is_number_shown) {
        write_numbers(writer);
        writer.write_inner_html(u8". "sv);
    }
    writer.write_inner_html(heading_html_string);
    writer.close_tag(tag_name);

    // 6. Also write an ID preview in case the heading is referenced via \ref[#id]

    if (has_valid_id) {
        Document_Sections& sections = context.get_sections();
        std::pmr::u8string section_name { context.get_transient_memory() };
        section_name += section_name::id_preview;
        section_name += u8'.';
        section_name += id.string;

        const auto scope = sections.go_to_scoped(section_name);
        HTML_Writer_Buffer id_preview_buffer { sections.current_policy(), Output_Language::html };
        Text_Buffer_HTML_Writer id_preview_out { id_preview_buffer };
        id_preview_out.write_inner_html(u8"§"sv);
        if (is_listed && is_number_shown) {
            write_numbers(id_preview_out);
            id_preview_out.write_inner_html(u8". "sv);
        }
        else {
            id_preview_out.write_inner_html(u8' ');
        }
        id_preview_out.write_inner_html(heading_html_string);
        id_preview_buffer.flush();
    }

    // 7. If necessary, also output the heading into the table of contents.
    if (is_listed) {
        Document_Sections& sections = context.get_sections();
        const auto scope = sections.go_to_scoped(section_name::table_of_contents);
        HTML_Writer_Buffer toc_buffer { sections.current_policy(), Output_Language::html };
        Text_Buffer_HTML_Writer toc_writer { toc_buffer };

        toc_writer
            .open_tag_with_attributes(html_tag::div) //
            .write_class(u8"toc-num"sv)
            .write_attribute(html_attr::data_level, tag_name.str().substr(1))
            .end();
        write_numbers(toc_writer);
        toc_writer.close_tag(html_tag::div);
        toc_writer.write_inner_html(u8'\n'); // non-functional, purely for prettier HTML output

        if (has_valid_id) {
            toc_writer
                .open_tag_with_attributes(html_tag::a) //
                .write_url_attribute(html_attr::href, joined_char_sequence({ u8"#"sv, id.string }))
                .end();
        }

        toc_writer.open_tag(tag_name);
        toc_writer.write_inner_html(heading_html_string);
        toc_writer.close_tag(tag_name);

        if (has_valid_id) {
            toc_writer.close_tag(html_tag::a);
        }
        toc_writer.write_inner_html(u8'\n'); // non-functional, purely for prettier HTML output
        toc_buffer.flush();
    }

    return current_status;
}

namespace {

[[nodiscard]]
Processing_Status with_section_name(
    Content_Policy& out,
    const Invocation& call,
    Context& context,
    std::u8string_view no_section_diagnostic,
    Function_Ref<Processing_Status(std::u8string_view section)> action
)
{
    String_Matcher section_matcher { context.get_transient_memory() };
    Group_Member_Matcher section_member { u8"section"sv, Optionality::mandatory, section_matcher };
    Group_Member_Matcher* parameters[] { &section_member };
    Pack_Usual_Matcher args_matcher { parameters };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const auto match_status = call_matcher.match_call(call, context, make_fail_callback());
    if (match_status != Processing_Status::ok) {
        return status_is_error(match_status) ? try_generate_error(out, call, context, match_status)
                                             : match_status;
    }

    const auto section_string = as_u8string_view(section_matcher.get());
    if (section_string.empty()) {
        context.try_error(
            no_section_diagnostic, call.directive.get_source_span(), u8"No section was provided."sv
        );
        return Processing_Status::error;
    }

    return action(section_string);
}

} // namespace

Processing_Status
There_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    const auto action = [&](std::u8string_view section) -> Processing_Status {
        const auto scope = context.get_sections().go_to_scoped(section);
        return consume_all(
            context.get_sections().current_policy(), call.get_content_span(), call.content_frame,
            context
        );
    };
    return with_section_name(out, call, context, diagnostic::there::no_section, action);
}

Processing_Status
Here_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    ensure_paragraph_matches_display(out, m_display);

    const auto action = [&](std::u8string_view section) -> Processing_Status {
        reference_section(out, section);
        return Processing_Status::ok;
    };
    return with_section_name(out, call, context, diagnostic::here::no_section, action);
}

Processing_Status
Make_Section_Behavior::operator()(Content_Policy& out, const Invocation&, Context& context) const
{
    ensure_paragraph_matches_display(out, m_display);

    context.get_sections().make(m_section_name);

    // TODO: warn about ignored arguments and block
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    writer
        .open_tag_with_attributes(html_tag::div) //
        .write_class(m_class_name)
        .end();
    reference_section(buffer, m_section_name);
    writer.close_tag(html_tag::div);
    return Processing_Status::ok;
}

} // namespace cowel

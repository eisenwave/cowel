#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
#include "cowel/directive_arguments.hpp"
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
    static constexpr std::u8string_view parameters[] { u8"id", u8"listed", u8"show-number" };

    const auto level_char = char8_t(int(u8'0') + m_level);
    const char8_t tag_name_data[2] { u8'h', level_char };
    const HTML_Tag_Name tag_name { std::u8string_view { tag_name_data, sizeof(tag_name_data) } };

    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(call.arguments, Parameter_Match_Mode::only_named);

    // Determine whether the heading should be listed in the table of contents.
    const bool listed_by_default = m_level >= min_listing_level && m_level <= max_listing_level;
    const Greedy_Result<bool> is_listed_result = get_yes_no_argument(
        u8"listed", diagnostic::h::listed_invalid, call.arguments, args, context, listed_by_default
    );
    if (status_is_break(is_listed_result.status())) {
        return is_listed_result.status();
    }

    const Greedy_Result<bool> is_number_shown_result = get_yes_no_argument(
        u8"show-number", diagnostic::h::show_number_invalid, call.arguments, args, context,
        listed_by_default
    );
    if (status_is_break(is_number_shown_result.status())) {
        return is_number_shown_result.status();
    }
    Processing_Status current_status
        = status_concat(is_listed_result.status(), is_number_shown_result.status());

    // TODO: now that we use Greedy_Result, we should be able to get rid of these
    //       extra variables, but it's not worth the trouble right now ...
    const bool is_listed = is_listed_result ? *is_listed_result : listed_by_default;
    const bool is_number_shown
        = is_number_shown_result ? *is_number_shown_result : listed_by_default;

    if (is_listed) {
        // Update heading numbers.
        COWEL_ASSERT(m_level >= 1 && m_level <= 6);
        ++h_counters[m_level - 1];
        std::ranges::fill(h_counters + m_level, std::end(h_counters), 0);
    }

    std::pmr::vector<char8_t> id_data { context.get_transient_memory() };
    const auto id_status = [&] -> Processing_Status {
        const int id_index = args.get_argument_index(u8"id");
        if (id_index < 0) {
            return synthesize_id(id_data, call.content, call.content_frame, context);
        }
        const Argument_Ref id_arg = call.arguments[std::size_t(id_index)];
        return to_plaintext(id_data, id_arg.ast_node.get_content(), id_arg.frame_index, context);
    }();
    current_status = status_concat(current_status, id_status);
    if (status_is_break(id_status)) {
        return current_status;
    }
    const bool has_id = id_status == Processing_Status::ok && !id_data.empty();

    warn_ignored_argument_subset(
        call.arguments, args, context, Argument_Subset::unmatched_positional
    );

    // 0. Ensure that headings are not in paragraphs.
    try_leave_paragraph(out);

    // 1. Write HTML attributes, including the (possibly synthesized) id.
    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };
    auto attributes = writer.open_tag_with_attributes(tag_name);
    if (has_id) {
        attributes.write_id(as_u8string_view(id_data));
    }
    const auto attributes_status = named_arguments_to_attributes(
        attributes, call.arguments, args, context, Argument_Subset::unmatched_named
    );
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
            = consume_all(html_policy, call.content, call.content_frame, context);
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
        context.try_warning(
            diagnostic::duplicate_id, call.directive.get_source_span(),
            joined_char_sequence(message)
        );
        return false;
    }();

    // 4. Surround user content with paragraph/anchor link.
    if (has_valid_id) {
        id_data.insert(id_data.begin(), u8'#');
        writer
            .open_tag_with_attributes(html_tag::a) //
            .write_class(u8"para"sv)
            .write_url_attribute(html_attr::href, as_u8string_view(id_data))
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
        COWEL_ASSERT(id_data.front() == u8'#');
        section_name += as_u8string_view(id_data).substr(1);

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
                .write_url_attribute(html_attr::href, as_u8string_view(id_data))
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
    const Invocation& call,
    Context& context,
    std::u8string_view no_section_diagnostic,
    Function_Ref<Processing_Status(std::u8string_view section)> action
)
{
    static constexpr std::u8string_view parameters[] { u8"section" };
    Argument_Matcher args { parameters, context.get_persistent_memory() };
    args.match(call.arguments);

    const int arg_index = args.get_argument_index(u8"section");
    if (arg_index < 0) {
        context.try_error(
            no_section_diagnostic, call.directive.get_source_span(), u8"No section was provided."sv
        );
        return Processing_Status::error;
    }

    std::pmr::vector<char8_t> name_data { context.get_transient_memory() };
    const Argument_Ref arg = call.arguments[std::size_t(arg_index)];
    const auto name_status
        = to_plaintext(name_data, arg.ast_node.get_content(), arg.frame_index, context);
    if (name_status != Processing_Status::ok) {
        return name_status;
    }

    const auto section_string = as_u8string_view(name_data);
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
There_Behavior::operator()(Content_Policy&, const Invocation& call, Context& context) const
{
    auto action = [&](std::u8string_view section) {
        const auto scope = context.get_sections().go_to_scoped(section);
        return consume_all(
            context.get_sections().current_policy(), call.content, call.content_frame, context
        );
    };
    return with_section_name(call, context, diagnostic::there::no_section, action);
}

Processing_Status
Here_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    ensure_paragraph_matches_display(out, m_display);

    auto action = [&](std::u8string_view section) {
        reference_section(out, section);
        return Processing_Status::ok;
    };
    return with_section_name(call, context, diagnostic::there::no_section, action);
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

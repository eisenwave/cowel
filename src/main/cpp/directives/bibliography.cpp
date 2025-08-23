#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/strings.hpp"
#include "cowel/util/url_encode.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/ast.hpp"
#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/invocation.hpp"
#include "cowel/parameters.hpp"
#include "cowel/services.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

template <typename Predicate>
void url_encode_to_writer(
    Text_Buffer_HTML_Writer& out,
    std::u8string_view url,
    Context& context,
    Predicate pred
)
{
    std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
    url_encode_ascii_if(std::back_inserter(buffer), url, pred);
    out.write_inner_html(as_u8string_view(buffer));
}

constexpr std::u8string_view bib_item_id_prefix = u8"bib-item-";

constexpr auto is_url_always_encoded_lambda = [](char8_t c) { return is_url_always_encoded(c); };

void write_bibliography_entry(
    Text_Buffer_HTML_Writer& out,
    const Document_Info& info,
    Context& context
)
{
    const auto open_link_tag = [&](std::u8string_view url, bool link_class = false) {
        out.write_inner_html(u8"<a href=\""sv);
        url_encode_to_writer(out, url, context, is_url_always_encoded_lambda);
        out.write_inner_html(u8'"');
        if (link_class) {
            out.write_inner_html(u8" class=bib-link"sv);
        }
        out.write_inner_html(u8'>');
    };

    const std::u8string_view id_parts[] { bib_item_id_prefix, info.id };

    out.open_tag_with_attributes(html_tag::div) //
        .write_id(joined_char_sequence(id_parts))
        .write_class(u8"bib-item"sv)
        .end();
    out.write_inner_html(u8'\n');

    if (!info.link.empty()) {
        open_link_tag(info.link);
    }

    COWEL_ASSERT(!info.id.empty());
    out.write_inner_html(u8'[');
    out.write_inner_text(info.id);
    out.write_inner_html(u8']');

    if (!info.link.empty()) {
        out.write_inner_html(u8"</a>"sv);
    }

    if (!info.author.empty()) {
        out.write_inner_html(u8'\n');
        out.open_tag_with_attributes(html_tag::span) //
            .write_class(u8"bib-author"sv)
            .end();
        out.write_inner_text(info.author);
        out.write_inner_html(u8'.');
        out.close_tag(html_tag::span);
    }
    if (!info.title.empty()) {
        out.write_inner_html(u8'\n');
        out.open_tag_with_attributes(html_tag::span) //
            .write_class(u8"bib-title"sv)
            .end();
        out.write_inner_text(info.title);
        out.close_tag(html_tag::span);
    }
    if (!info.date.empty()) {
        out.write_inner_html(u8"\n"sv);
        out.open_tag_with_attributes(html_tag::span) //
            .write_class(u8"bib-date"sv)
            .end();
        out.write_inner_text(info.date);
        out.close_tag(html_tag::span);
    }

    const auto write_link = [&](std::u8string_view url) {
        out.write_inner_html(u8'\n');
        open_link_tag(url, true);
        out.write_inner_text(url);
        out.write_inner_html(u8"</a>"sv);
    };

    if (!info.long_link.empty()) {
        write_link(info.long_link);
    }
    else if (!info.link.empty()) {
        write_link(info.link);
    }

    out.close_tag(html_tag::div);
}

} // namespace

Processing_Status
Bibliography_Add_Behavior::operator()(Content_Policy&, const Invocation& call, Context& context)
    const
{
    auto* memory = context.get_transient_memory();

    String_Matcher id_string { memory };
    Group_Member_Matcher id_member { u8"id", Optionality::mandatory, id_string };
    String_Matcher title_string { memory };
    Group_Member_Matcher title_member { u8"title", Optionality::optional, title_string };
    String_Matcher date_string { memory };
    Group_Member_Matcher date_member { u8"date", Optionality::optional, date_string };
    String_Matcher publisher_string { memory };
    Group_Member_Matcher publisher_member { u8"publisher", Optionality::optional,
                                            publisher_string };
    String_Matcher link_string { memory };
    Group_Member_Matcher link_member { u8"link", Optionality::optional, link_string };
    String_Matcher long_link_string { memory };
    Group_Member_Matcher long_link_member { u8"long-link", Optionality::optional,
                                            long_link_string };
    String_Matcher author_string { memory };
    Group_Member_Matcher author_member { u8"author", Optionality::optional, author_string };

    Group_Member_Matcher* const matchers[] {
        &id_member,   &title_member,     &date_member,   &publisher_member,
        &link_member, &long_link_member, &author_member,
    };
    Pack_Usual_Matcher args_matcher { matchers };
    Group_Pack_Matcher group_matcher { args_matcher };
    Call_Matcher call_matcher { group_matcher };

    const Processing_Status match_status
        = call_matcher.match_call(call, context, make_fail_callback(), Processing_Status::error);
    if (match_status != Processing_Status::ok) {
        return match_status;
    }

    if (id_string.get().empty()) {
        context.try_error(
            diagnostic::bib::id_empty, call.directive.get_source_span(),
            u8"An id argument for a bibliography entry cannot be empty."sv
        );
        return Processing_Status::error;
    }

    const Document_Info info {
        .id = id_string.get(),
        .title = title_string.get_or_default(u8""),
        .date = date_string.get_or_default(u8""),
        .publisher = publisher_string.get_or_default(u8""),
        .link = link_string.get_or_default(u8""),
        .long_link = long_link_string.get_or_default(u8""),
        .issue_link = {},
        .author = author_string.get_or_default(u8""),
    };

    // To facilitate later referencing,
    // we output the opening and closing HTML tags for this bibliography entry into sections.
    // If the bibliography entry has a link,
    // those tags will be "<a href=..." and "</a>",
    // otherwise the sections remain empty.
    {
        std::pmr::u8string section_name { u8"std.bib.", context.get_transient_memory() };
        section_name += info.id;
        const auto scope = context.get_sections().go_to_scoped(section_name);
        Content_Policy& section_out = context.get_sections().current_policy();
        HTML_Writer_Buffer buffer { section_out, Output_Language::html };
        Text_Buffer_HTML_Writer section_writer { buffer };

        if (!info.link.empty()) {
            // If the document info has a link,
            // then we want references to bibliography entries (e.g. "[N5008]")
            // to use that link.
            section_writer.write_inner_html(u8"<a href=\""sv);
            url_encode_to_writer(section_writer, info.link, context, is_url_always_encoded_lambda);
            section_writer.write_inner_html(u8"\">"sv);
        }
        else {
            // Otherwise, the reference should redirect down
            // to the respective entry within the bibliography.
            // In any case, it's important to guarantee that an <a> element will be emitted.
            const std::u8string_view id_parts[] { u8"#"sv, bib_item_id_prefix, info.id };
            section_writer
                .open_tag_with_attributes(html_tag::a) //
                .write_id(joined_char_sequence(id_parts))
                .end();
            section_writer.set_depth(0);
        }
        buffer.flush();
    }

    {
        const auto scope = context.get_sections().go_to_scoped(section_name::bibliography);
        HTML_Writer_Buffer buffer { context.get_sections().current_policy(),
                                    Output_Language::html };
        Text_Buffer_HTML_Writer bib_writer { buffer };
        write_bibliography_entry(bib_writer, info, context);
        buffer.flush();
    }

    return Processing_Status::ok;
}

} // namespace cowel

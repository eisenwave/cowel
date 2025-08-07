#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_factory.hpp"
#include "cowel/util/draft_uris.hpp"
#include "cowel/util/html_writer.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/content_status.hpp"
#include "cowel/context.hpp"
#include "cowel/diagnostic.hpp"
#include "cowel/directive_arguments.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/document_sections.hpp"
#include "cowel/fwd.hpp"
#include "cowel/invocation.hpp"

using namespace std::string_view_literals;

namespace cowel {
namespace {

enum struct Reference_Type : Default_Underlying {
    /// @brief An unclassifiable kind of reference.
    unknown,
    /// @brief A URL, like `http://google.com`.
    url,
    /// @brief An anchor with no preceding URL, like `#section`.
    anchor,
};

enum struct URL_Scheme : Default_Underlying {
    /// @brief URL with unknown or no scheme, like `//google.com`.
    none,
    http,
    https,
    tel,
    mailto,
};

[[nodiscard]]
constexpr bool url_scheme_is_web(URL_Scheme scheme)
{
    return scheme == URL_Scheme::none || scheme == URL_Scheme::http || scheme == URL_Scheme::https;
}

[[nodiscard]]
constexpr std::u8string_view url_scheme_prefix(URL_Scheme scheme)
{
    using enum URL_Scheme;
    switch (scheme) {
    case http: return u8"http:";
    case https: return u8"https:";
    case tel: return u8"tel:";
    case mailto: return u8"mailto:";
    default: return u8"";
    }
}

enum struct Known_Page : Default_Underlying {
    /// @brief `https://eel.is/c++draft/`
    eelis_draft,
};

struct Reference_Classification {
    Reference_Type type = Reference_Type::unknown;
    URL_Scheme url_scheme = URL_Scheme::none;
    std::optional<Known_Page> page = {};
};

Reference_Classification classify_reference(std::u8string_view ref)
{
    if (ref.starts_with(u8'#')) {
        return { Reference_Type::anchor };
    }

    const auto classify_web_url = [&](URL_Scheme scheme) -> Reference_Classification {
        const std::u8string_view prefix = url_scheme_prefix(scheme);
        COWEL_DEBUG_ASSERT(ref.starts_with(prefix));
        ref.remove_prefix(prefix.length());

        std::optional<Known_Page> page;
        if (ref.starts_with(u8"//eel.is/c++draft/")) {
            page.emplace(Known_Page::eelis_draft);
        }
        return { Reference_Type::url, scheme, page };
    };

    if (ref.starts_with(url_scheme_prefix(URL_Scheme::http))) {
        return classify_web_url(URL_Scheme::http);
    }
    if (ref.starts_with(url_scheme_prefix(URL_Scheme::https))) {
        return classify_web_url(URL_Scheme::https);
    }
    if (ref.starts_with(u8"//")) {
        return classify_web_url(URL_Scheme::none);
    }

    if (ref.starts_with(url_scheme_prefix(URL_Scheme::tel))) {
        return { Reference_Type::url, URL_Scheme::tel };
    }
    if (ref.starts_with(url_scheme_prefix(URL_Scheme::mailto))) {
        return { Reference_Type::url, URL_Scheme::mailto };
    }

    return {};
}

} // namespace

Processing_Status
Ref_Behavior::operator()(Content_Policy& out, const Invocation& call, Context& context) const
{
    static const std::u8string_view parameters[] { u8"to" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(call.arguments);

    for (std::size_t i = 0; i < args.argument_statuses().size(); ++i) {
        if (args.argument_statuses()[i] == Argument_Status::unmatched) {
            context.try_warning(
                diagnostic::ignored_args, call.arguments[i].ast_node.get_source_span(),
                u8"This argument was ignored."sv
            );
        }
    }

    const int to_index = args.get_argument_index(u8"to");
    if (to_index < 0) {
        context.try_error(
            diagnostic::ref::to_missing, call.directive.get_source_span(),
            u8"A \"to\" argument is required for a reference."sv
        );
        return try_generate_error(out, call, context);
    }

    std::pmr::vector<char8_t> target { context.get_transient_memory() };
    const Argument_Ref to_arg = call.arguments[std::size_t(to_index)];
    const auto target_status
        = to_plaintext(target, to_arg.ast_node.get_content(), to_arg.frame_index, context);
    if (target_status != Processing_Status::ok) {
        return target_status;
    }
    if (target.empty()) {
        context.try_error(
            diagnostic::ref::to_empty, call.directive.get_source_span(),
            u8"A \"to\" argument cannot have an empty value."sv
        );
        return try_generate_error(out, call, context);
    }

    try_enter_paragraph(out);

    HTML_Writer_Buffer buffer { out, Output_Language::html };
    Text_Buffer_HTML_Writer writer { buffer };

    const auto target_string = as_u8string_view(target);
    const Reference_Classification classification = classify_reference(target_string);
    if (classification.type == Reference_Type::unknown) {
        const std::u8string_view section_name_parts[] {
            section_name::bibliography,
            u8".",
            target_string,
        };
        reference_section(buffer, joined_char_sequence(section_name_parts));
        if (call.content.empty()) {
            writer.write_inner_html(u8'[');
            writer.write_inner_text(target_string);
            writer.write_inner_html(u8']');
        }
        writer.write_inner_html(u8"</a>"sv); // no close_tag to avoid depth check
        buffer.flush();
        return Processing_Status::ok;
    }

    if (classification.type == Reference_Type::anchor) {
        writer
            .open_tag_with_attributes(html_tag::a) //
            .write_href(target_string)
            .end();
        auto status = Processing_Status::ok;
        if (call.content.empty()) {
            const std::u8string_view section_name_parts[] {
                section_name::id_preview,
                u8".",
                target_string.substr(1),
            };
            reference_section(buffer, joined_char_sequence(section_name_parts));
        }
        else {
            buffer.flush();
            status = consume_all(out, call.content, call.content_frame, context);
        }
        writer.close_tag(html_tag::a);
        buffer.flush();
        return status;
    }

    COWEL_ASSERT(classification.type == Reference_Type::url);

    if (!call.content.empty()) {
        writer
            .open_tag_with_attributes(html_tag::a) //
            .write_href(target_string)
            .end();
        buffer.flush();
        const auto status = consume_all(out, call.content, call.content_frame, context);
        writer.close_tag(html_tag::a);
        buffer.flush();
        return status;
    }

    auto attributes = writer.open_tag_with_attributes(html_tag::a);
    attributes.write_href(target_string);
    const bool is_sans = classification.url_scheme == URL_Scheme::mailto
        || classification.url_scheme == URL_Scheme::tel
        || (url_scheme_is_web(classification.url_scheme)
            && classification.page != Known_Page::eelis_draft);
    if (is_sans) {
        attributes.write_class(u8"sans"sv);
    }
    attributes.end();

    if (classification.page != Known_Page::eelis_draft) {
        if (classification.url_scheme != URL_Scheme::none) {
            const std::size_t colon_index = target_string.find(u8':');
            auto text = colon_index == std::u8string_view::npos
                ? target_string
                : target_string.substr(colon_index + 1);
            if (url_scheme_is_web(classification.url_scheme) && text.starts_with(u8"//")) {
                text.remove_prefix(2);
            }
            writer.write_inner_text(text);
        }
        else {
            writer.write_inner_text(target_string);
        }
        writer.close_tag(html_tag::a);
        buffer.flush();
        return Processing_Status::ok;
    }

    const std::size_t last_slash_pos = target_string.find_last_of(u8'/');
    // Classification as eel.is URL should have been impossible if there is no slash.
    COWEL_ASSERT(last_slash_pos != std::u8string_view::npos);
    const std::u8string_view last_uri_part = target_string.substr(last_slash_pos + 1);

    auto consume_verbalized = [&](std::u8string_view part, Text_Format format) {
        switch (format) {
        case Text_Format::section:
            writer.write_inner_html(u8'[');
            writer.write_inner_text(part);
            writer.write_inner_html(u8']');
            break;
        case Text_Format::grammar:
            writer.open_tag(html_tag::g_term);
            writer.write_inner_text(part);
            writer.close_tag(html_tag::g_term);
            break;
        case Text_Format::code:
            writer.open_tag(html_tag::tt_);
            writer.write_inner_text(part);
            writer.close_tag(html_tag::tt_);
            break;
        default: //
            writer.write_inner_text(part);
            break;
        }
    };
    Draft_Location location_buffer[16];
    const Result<void, Draft_URI_Error> r
        = parse_and_verbalize_draft_uri(consume_verbalized, last_uri_part, location_buffer);
    if (!r) {
        const std::u8string_view message[] {
            u8"The given reference in the C++ draft \"",
            last_uri_part,
            u8"\" could not be verbalized automatically.",
        };
        context.try_warning(
            diagnostic::ref::draft_verbalization, call.directive.get_source_span(),
            joined_char_sequence(message)
        );
        writer.write_inner_text(target_string);
    }
    writer.close_tag(html_tag::a);
    return Processing_Status::ok;
}

} // namespace cowel

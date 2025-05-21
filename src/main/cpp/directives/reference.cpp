#include <string_view>
#include <vector>

#include "cowel/util/draft_uris.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_processing.hpp"

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

void Ref_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    static const std::u8string_view parameters[] { u8"to" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments(), context.get_source());

    for (std::size_t i = 0; i < args.argument_statuses().size(); ++i) {
        if (args.argument_statuses()[i] == Argument_Status::unmatched) {
            context.try_warning(
                diagnostic::ref_args_ignored, d.get_arguments()[i].get_source_span(),
                u8"This argument was ignored."
            );
        }
    }

    const int to_index = args.get_argument_index(u8"to");
    if (to_index < 0) {
        context.try_error(
            diagnostic::ref_to_missing, d.get_source_span(),
            u8"A \"to\" argument is required for a reference."
        );
        try_generate_error_html(out, d, context);
        return;
    }

    std::pmr::vector<char8_t> target { context.get_transient_memory() };
    to_plaintext(target, d.get_arguments()[std::size_t(to_index)].get_content(), context);
    if (target.empty()) {
        context.try_error(
            diagnostic::ref_to_empty, d.get_source_span(),
            u8"A \"to\" argument cannot have an empty value."
        );
        try_generate_error_html(out, d, context);
        return;
    }

    const auto target_string = as_u8string_view(target);
    const Reference_Classification classification = classify_reference(target_string);
    if (classification.type == Reference_Type::unknown) {
        std::pmr::u8string section_name { context.get_transient_memory() };
        section_name += section_name::bibliography;
        section_name += u8'.';
        section_name += target_string;
        reference_section(out, section_name);
        if (d.get_content().empty()) {
            out.write_inner_html(u8'[');
            out.write_inner_text(target_string);
            out.write_inner_html(u8']');
        }
        out.write_inner_html(u8"</a>"); // no close_tag to avoid depth check
        return;
    }

    if (classification.type == Reference_Type::anchor) {
        out.open_tag_with_attributes(u8"a") //
            .write_href(target_string)
            .end();
        if (d.get_content().empty()) {
            std::pmr::u8string section_name { context.get_transient_memory() };
            section_name += section_name::id_preview;
            section_name += u8'.';
            section_name += target_string.substr(1);
            reference_section(out, section_name);
        }
        else {
            to_html(out, d.get_content(), context);
        }
        out.close_tag(u8"a");
        return;
    }

    COWEL_ASSERT(classification.type == Reference_Type::url);

    if (!d.get_content().empty()) {
        out.open_tag_with_attributes(u8"a") //
            .write_href(target_string)
            .end();
        to_html(out, d.get_content(), context);
        out.close_tag(u8"a");
        return;
    }

    Attribute_Writer attributes = out.open_tag_with_attributes(u8"a");
    attributes.write_href(target_string);
    const bool is_sans = classification.url_scheme == URL_Scheme::mailto
        || classification.url_scheme == URL_Scheme::tel
        || (url_scheme_is_web(classification.url_scheme)
            && classification.page != Known_Page::eelis_draft);
    if (is_sans) {
        attributes.write_class(u8"sans");
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
            out.write_inner_text(text);
        }
        else {
            out.write_inner_text(target_string);
        }
        out.close_tag(u8"a");
        return;
    }

    const std::size_t last_slash_pos = target_string.find_last_of(u8'/');
    // Classification as eel.is URL should have been impossible if there is no slash.
    COWEL_ASSERT(last_slash_pos != std::u8string_view::npos);
    const std::u8string_view last_uri_part = target_string.substr(last_slash_pos + 1);

    auto consume_verbalized = [&](std::u8string_view part, Text_Format format) {
        switch (format) {
        case Text_Format::section:
            out.write_inner_html(u8'[');
            out.write_inner_text(part);
            out.write_inner_html(u8']');
            break;
        case Text_Format::grammar:
            out.open_tag(u8"g-term");
            out.write_inner_text(part);
            out.close_tag(u8"g-term");
            break;
        case Text_Format::code:
            out.open_tag(u8"tt-");
            out.write_inner_text(part);
            out.close_tag(u8"tt-");
            break;
        default: //
            out.write_inner_text(part);
            break;
        }
    };
    Draft_Location buffer[16];
    const Result<void, Draft_URI_Error> r
        = parse_and_verbalize_draft_uri(consume_verbalized, last_uri_part, buffer);
    if (!r) {
        if (context.emits(Severity::warning)) {
            Diagnostic warning
                = context.make_warning(diagnostic::ref_draft_verbalization, d.get_source_span());
            warning.message += u8"The given reference in the C++ draft \"";
            warning.message += last_uri_part;
            warning.message += u8"\" could not be verbalized automatically.";
            context.emit(std::move(warning));
        }
        out.write_inner_text(target_string);
    }
    out.close_tag(u8"a");
}

} // namespace cowel

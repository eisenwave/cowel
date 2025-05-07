#include "mmml/builtin_directive_set.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/util/draft_uris.hpp"
#include "mmml/util/strings.hpp"

namespace mmml {

void Wrap_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    to_plaintext(out, d.get_content(), context);
}

void Wrap_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    to_html(out, d.get_content(), context);
}

void Passthrough_Behavior::generate_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
) const
{
    to_plaintext(out, d.get_content(), context);
}

void Passthrough_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    const std::u8string_view name = get_name(d, context);
    if (d.get_arguments().empty()) {
        out.open_tag(name);
    }
    else {
        Attribute_Writer attributes = out.open_tag_with_attributes(name);
        arguments_to_attributes(attributes, d, context);
    }
    to_html(out, d.get_content(), context);
    out.close_tag(name);
}

void In_Tag_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.write_class(m_class_name);
    attributes.end();

    to_html(out, d.get_content(), context);
    out.close_tag(m_tag_name);
}

[[nodiscard]]
std::u8string_view
Directive_Name_Passthrough_Behavior::get_name(const ast::Directive& d, Context& context) const
{
    const std::u8string_view raw_name = d.get_name(context.get_source());
    const std::u8string_view name
        = raw_name.starts_with(builtin_directive_prefix) ? raw_name.substr(1) : raw_name;
    return name.substr(m_name_prefix.size());
}

void Special_Block_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    if (d.get_arguments().empty()) {
        out.open_tag(m_name);
    }
    else {
        Attribute_Writer attributes = out.open_tag_with_attributes(m_name);
        arguments_to_attributes(attributes, d, context);
    }
    out.open_tag(u8"p");
    if (m_emit_intro) {
        out.open_and_close_tag(u8"intro-");
        // This space ensures that even if the user writes say,
        // \note{abc}, there is a space between </into> and abc.
        out.write_inner_html(u8' ');
    }
    to_html(out, d.get_content(), context, To_HTML_Mode::paragraphs, Paragraphs_State::inside);
    out.close_tag(m_name);
}

void WG21_Block_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    constexpr std::u8string_view tag = u8"wg21-block";

    Attribute_Writer attributes = out.open_tag_with_attributes(tag);
    arguments_to_attributes(attributes, d, context);
    attributes.end();

    out.write_inner_html(u8"[<i>");
    out.write_inner_text(m_prefix);
    out.write_inner_html(u8"</i>: ");

    to_html(out, d.get_content(), context);

    out.write_inner_html(u8" \N{EM DASH} <i>");
    out.write_inner_text(m_suffix);
    out.write_inner_html(u8"</i>]");
    out.close_tag(tag);
}

void WG21_Head_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context)
    const
{
    static constexpr std::u8string_view parameters[] { u8"title" };
    Argument_Matcher args { parameters, context.get_transient_memory() };
    args.match(d.get_arguments(), context.get_source());

    out.open_tag_with_attributes(u8"div") //
        .write_class(u8"wg21-head")
        .end();

    const int title_index = args.get_argument_index(u8"title");
    if (title_index < 0) {
        context.try_warning(
            diagnostic::wg21_head_no_title, d.get_source_span(),
            u8"A wg21-head directive requires a title argument"
        );
    }
    out.open_tag(u8"h1");
    to_html(out, d.get_arguments()[std::size_t(title_index)].get_content(), context);
    out.close_tag(u8"h1");
    out.write_inner_html(u8'\n');

    to_html(out, d.get_content(), context);

    out.close_tag(u8"div");
}

void URL_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    std::pmr::vector<char8_t> url { context.get_transient_memory() };
    append(url, m_url_prefix);
    to_plaintext(url, d.get_content(), context);
    const auto url_string = as_u8string_view(url);

    Attribute_Writer attributes = out.open_tag_with_attributes(u8"a");
    arguments_to_attributes(attributes, d, context);
    attributes.write_href(url_string);
    attributes.write_class(u8"sans");
    attributes.end();

    MMML_ASSERT(url_string.length() >= m_url_prefix.length());
    out.write_inner_text(url_string.substr(m_url_prefix.length()));

    out.close_tag(u8"a");
}

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
        MMML_DEBUG_ASSERT(ref.starts_with(prefix));
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
        if (context.emits(Severity::error)) {
            Diagnostic error
                = context.make_error(diagnostic::ref_to_unclassified, d.get_source_span());
            error.message += u8"The specified target \"";
            error.message += target_string;
            error.message += u8"\" cannot be classified as URL or anything else, "
                             "so the reference is invalid.";
            context.emit(std::move(error));
        }
        try_generate_error_html(out, d, context);
        return;
    }

    if (classification.type == Reference_Type::anchor) {
        out.open_tag_with_attributes(u8"a") //
            .write_href(target_string)
            .end();
        if (d.get_content().empty()) {
            // TODO: generate preview from ID
            out.write_inner_text(target_string);
        }
        else {
            to_html(out, d.get_content(), context);
        }
        out.close_tag(u8"a");
        return;
    }

    MMML_ASSERT(classification.type == Reference_Type::url);
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

    if (!d.get_content().empty()) {
        to_html(out, d.get_content(), context);
        out.close_tag(u8"a");
        return;
    }

    if (classification.page != Known_Page::eelis_draft) {
        out.write_inner_text(target_string);
        out.close_tag(u8"a");
        return;
    }

    const std::size_t last_slash_pos = target_string.find_last_of(u8'/');
    // Classification as eel.is URL should have been impossible if there is no slash.
    MMML_ASSERT(last_slash_pos != std::u8string_view::npos);
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

void Self_Closing_Behavior::generate_html(
    HTML_Writer& out,
    const ast::Directive& d,
    Context& context
) const
{
    if (!d.get_content().empty()) {
        const auto location = ast::get_source_span(d.get_content().front());
        context.try_warning(
            m_content_ignored_diagnostic, location,
            u8"Content was ignored. Use empty braces,"
            "i.e. {} to resolve this warning."
        );
    }

    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.end_empty();
}

void List_Behavior::generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const
{
    static Fixed_Name_Passthrough_Behavior item_behavior { u8"li", Directive_Category::pure_html,
                                                           Directive_Display::block };

    Attribute_Writer attributes = out.open_tag_with_attributes(m_tag_name);
    arguments_to_attributes(attributes, d, context);
    attributes.end();
    for (const ast::Content& c : d.get_content()) {
        if (const auto* const directive = std::get_if<ast::Directive>(&c)) {
            const std::u8string_view name = directive->get_name(context.get_source());
            if (name == u8"item" || name == u8"-item") {
                item_behavior.generate_html(out, *directive, context);
            }
            else {
                to_html(out, *directive, context);
            }
            continue;
        }
        to_html(out, c, context);
    }
    out.close_tag(m_tag_name);
}

} // namespace mmml

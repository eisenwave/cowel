#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mmml/util/chars.hpp"
#include "mmml/util/from_chars.hpp"
#include "mmml/util/html_entities.hpp"
#include "mmml/util/html_writer.hpp"
#include "mmml/util/strings.hpp"
#include "mmml/util/typo.hpp"

#include "mmml/ast.hpp"
#include "mmml/directive_arguments.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/directives.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

namespace {

struct Pure_HTML_Behavior : Directive_Behavior {

    Pure_HTML_Behavior(Directive_Display display)
        : Directive_Behavior { Directive_Category::pure_html, display }
    {
    }

    void generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, Context&) const final
    {
    }
};

struct Do_Nothing_Behavior : Directive_Behavior {
    // TODO: diagnose ignored arguments

    Do_Nothing_Behavior(Directive_Category category, Directive_Display display)
        : Directive_Behavior { category, display }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, Context&) const override
    {
    }

    void generate_html(HTML_Writer&, const ast::Directive&, Context&) const override { }
};

/// @brief Behavior for `\\error` directives.
/// Does not processing.
/// Generates no plaintext.
/// Generates HTML with the source code of the contents wrapped in an `<error->` custom tag.
struct Error_Behavior : Do_Nothing_Behavior {
    static constexpr std::u8string_view id = u8"error-";

    Error_Behavior()
        : Do_Nothing_Behavior { Directive_Category::pure_html, Directive_Display::in_line }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override
    {
        out.open_tag(id);
        for (const ast::Content& c : d.get_content()) {
            out.write_inner_text(get_source(c, context.get_source()));
        }
        out.close_tag(id);
    }
};

struct Passthrough_Behavior : Directive_Behavior {

    Passthrough_Behavior(Directive_Category category, Directive_Display display)
        : Directive_Behavior { category, display }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override
    {
        to_plaintext(out, d.get_content(), context);
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override
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

    [[nodiscard]]
    virtual std::u8string_view get_name(const ast::Directive& d, Context& context) const
        = 0;
};

constexpr char8_t builtin_directive_prefix = u8'-';

/// @brief Behavior for any formatting tags that are mapped onto HTML with the same name.
/// This includes `\\i{...}`, `\\strong`, and many more.
///
/// Preprocesses and processes all arguments.
/// Generates the contents inside in plaintext.
///
/// Generates HTML where arguments are converted to HTML attributes,
/// in a tag that has the same name as the directive.
/// For example, `\\i[id = 123]{...}` generates `<i id=123>...</i>`.
struct Directive_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const std::u8string_view m_name_prefix;

public:
    Directive_Name_Passthrough_Behavior(
        Directive_Category category,
        Directive_Display display,
        const std::u8string_view name_prefix = u8""
    )
        : Passthrough_Behavior { category, display }
        , m_name_prefix { name_prefix }
    {
    }

    [[nodiscard]]
    std::u8string_view get_name(const ast::Directive& d, Context& context) const override
    {
        const std::u8string_view raw_name = d.get_name(context.get_source());
        const std::u8string_view name
            = raw_name.starts_with(builtin_directive_prefix) ? raw_name.substr(1) : raw_name;
        return name.substr(m_name_prefix.size());
    }
};

struct Fixed_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const std::u8string_view m_name;

public:
    explicit Fixed_Name_Passthrough_Behavior(
        Directive_Category category,
        Directive_Display display,
        std::u8string_view name
    )
        : Passthrough_Behavior { category, display }
        , m_name { name }
    {
    }

    [[nodiscard]]
    std::u8string_view get_name(const ast::Directive&, Context&) const override
    {
        return m_name;
    }
};

/// @brief Behavior for the `\\html{...}`, directive.
/// This is a pure HTML directive.
///
/// Literals within this block are treated as HTML.
/// HTML generation takes place for any directives within.
struct HTML_Literal_Behavior : Pure_HTML_Behavior {

    HTML_Literal_Behavior()
        : Pure_HTML_Behavior(Directive_Display::block)
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override
    {
        std::pmr::vector<char8_t> buffer { context.get_transient_memory() };
        HTML_Writer buffer_writer { buffer };
        to_html_literally(buffer_writer, d.get_content(), context);
        const std::u8string_view buffer_string { buffer.data(), buffer.size() };
        out.write_inner_html(buffer_string);
    }
};

struct Parametric_Behavior : Directive_Behavior {
protected:
    const std::span<const std::u8string_view> m_parameters;

public:
    Parametric_Behavior(
        Directive_Category c,
        Directive_Display d,
        std::span<const std::u8string_view> parameters
    )
        : Directive_Behavior { c, d }
        , m_parameters { parameters }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override
    {
        Argument_Matcher args { m_parameters, context.get_transient_memory() };
        args.match(d.get_arguments(), context.get_source());
        generate_plaintext(out, d, args, context);
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override
    {
        Argument_Matcher args { m_parameters, context.get_transient_memory() };
        args.match(d.get_arguments(), context.get_source());
        generate_html(out, d, args, context);
    }

protected:
    virtual void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const
        = 0;
    virtual void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const
        = 0;
};

[[nodiscard]]
std::u8string_view argument_to_plaintext_or(
    std::pmr::vector<char8_t>& out,
    std::u8string_view parameter_name,
    std::u8string_view fallback,
    const ast::Directive& directive,
    const Argument_Matcher& args,
    Context& context
)
{
    const int index = args.get_argument_index(parameter_name);
    if (index < 0) {
        return fallback;
    }
    to_plaintext(out, directive.get_arguments()[std::size_t(index)].get_content(), context);
    return { out.data(), out.size() };
}

void try_generate_error_plaintext(
    std::pmr::vector<char8_t>& out,
    const ast::Directive& d,
    Context& context
)
{
    if (const Directive_Behavior* const behavior = context.get_error_behavior()) {
        behavior->generate_plaintext(out, d, context);
    }
}

void try_generate_error_html(HTML_Writer& out, const ast::Directive& d, Context& context)
{
    if (const Directive_Behavior* const behavior = context.get_error_behavior()) {
        behavior->generate_html(out, d, context);
    }
}

struct [[nodiscard]]
HTML_Entity_Behavior final : Directive_Behavior {
    HTML_Entity_Behavior()
        : Directive_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override
    {
        check_arguments(d, context);
        std::pmr::vector<char8_t> data { context.get_transient_memory() };
        to_plaintext(data, d.get_content(), context);
        const std::u8string_view trimmed_text = trim_ascii_blank({ data.data(), data.size() });
        const std::array<char32_t, 2> code_points = get_code_points(trimmed_text, d, context);
        if (code_points[0] == 0) {
            try_generate_error_plaintext(out, d, context);
            return;
        }
        HTML_Writer out_writer { out };
        out_writer.write_inner_html(as_string_view(code_points));
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final
    {
        check_arguments(d, context);
        std::pmr::vector<char8_t> data { context.get_transient_memory() };
        to_plaintext(data, d.get_content(), context);
        const std::u8string_view trimmed_text = trim_ascii_blank({ data.data(), data.size() });
        if (get_code_points(trimmed_text, d, context)[0] == 0) {
            try_generate_error_html(out, d, context);
            return;
        }
        out.write_inner_html(trimmed_text);
    }

private:
    [[nodiscard]]
    static std::u32string_view as_string_view(const std::array<char32_t, 2>& array)
    {
        if (array[0] == 0) {
            return { array.data(), 0uz };
        }
        if (array[1] == 0) {
            return { array.data(), 1uz };
        }
        return { array.data(), 2uz };
    }

    void check_arguments(const ast::Directive& d, Context& context) const
    {
        if (!d.get_arguments().empty()) {
            const Source_Span pos = d.get_arguments().front().get_source_span();
            context.try_emit(
                Severity::warning, u8"charref.args.ignored", pos,
                u8"Arguments to this directive are ignored."
            );
        }
    }

    [[nodiscard]]
    std::array<char32_t, 2>
    get_code_points(std::u8string_view trimmed_text, const ast::Directive& d, Context& context)
        const
    {
        if (trimmed_text.empty()) {
            context.try_emit(
                Severity::error, u8"charref.blank", d.get_source_span(),
                u8"Expected an HTML character reference, but got a blank string."
            );
            return {};
        }
        if (trimmed_text[0] == u8'#') {
            const int base
                = trimmed_text.starts_with(u8"#x") || trimmed_text.starts_with(u8"#X") ? 16 : 10;
            return get_code_points_from_digits(trimmed_text.substr(2), base, d, context);
        }
        const std::array<char32_t, 2> result
            = code_points_by_character_reference_name(trimmed_text);
        if (result[0] == 0) {
            context.try_emit(
                Severity::error, u8"charref.name", d.get_source_span(),
                u8"Invalid named HTML character."
            );
        }
        return result;
    }

    [[nodiscard]]
    std::array<char32_t, 2> get_code_points_from_digits(
        std::u8string_view digits,
        int base,
        const ast::Directive& d,
        Context& context
    ) const
    {
        std::optional<std::uint32_t> value = from_chars<std::uint32_t>(digits, base);
        if (!value) {
            const std::u8string_view message = base == 10
                ? u8"Expected a sequence of decimal digits."
                : u8"Expected a sequence of hexadecimal digits.";
            context.try_emit(Severity::error, u8"d.charref.digits", d.get_source_span(), message);
            return {};
        }

        const auto code_point = char32_t(*value);
        if (!is_scalar_value(code_point)) {
            context.try_emit(
                Severity::error, u8"charref.nonscalar", d.get_source_span(),
                u8"The given hex sequence is not a Unicode scalar value. "
                u8"Therefore, it cannot be encoded as UTF-8."
            );
            return {};
        }

        return { code_point };
    }
};

struct [[nodiscard]]
Code_Point_Behavior final : Directive_Behavior {
    static constexpr char32_t error_point = char32_t(-1);

    Code_Point_Behavior()
        : Directive_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override
    {
        const char32_t code_point = get_code_point(d, context);
        if (code_point == error_point) {
            try_generate_error_plaintext(out, d, context);
            return;
        }
        HTML_Writer out_writer { out };
        out_writer.write_inner_html(code_point);
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final
    {
        const char32_t code_point = get_code_point(d, context);
        if (code_point == error_point) {
            try_generate_error_html(out, d, context);
            return;
        }
        out.write_inner_html(code_point);
    }

    [[nodiscard]]
    char32_t get_code_point(const ast::Directive& d, Context& context) const
    {
        if (!d.get_arguments().empty()) {
            const Source_Span pos = d.get_arguments().front().get_source_span();
            context.try_emit(
                Severity::warning, u8"codepoint.args.ignored", pos,
                u8"Arguments to this directive are ignored."
            );
        }
        std::pmr::vector<char8_t> data { context.get_transient_memory() };
        to_plaintext(data, d.get_content(), context);
        const std::u8string_view digits = trim_ascii_blank({ data.data(), data.size() });
        if (digits.empty()) {
            context.try_emit(
                Severity::error, u8"codepoint.blank", d.get_source_span(),
                u8"Expected a sequence of hexadecimal digits, but got a blank string."
            );
            return error_point;
        }

        std::optional<std::uint32_t> value = from_chars<std::uint32_t>(digits, 16);
        if (!value) {
            context.try_emit(
                Severity::error, u8"codepoint.parse", d.get_source_span(),
                u8"Expected a sequence of hexadecimal digits."
            );
            return error_point;
        }

        const auto code_point = char32_t(*value);
        if (!is_scalar_value(code_point)) {
            context.try_emit(
                Severity::error, u8"codepoint.nonscalar", d.get_source_span(),
                u8"The given hex sequence is not a Unicode scalar value. "
                u8"Therefore, it cannot be encoded as UTF-8."
            );
            return error_point;
        }

        return code_point;
    }
};

struct [[nodiscard]] Syntax_Highlight_Behavior : Parametric_Behavior {
    static constexpr std::u8string_view lang_parameter = u8"lang";
    static constexpr std::u8string_view parameters[] { lang_parameter };

    explicit Syntax_Highlight_Behavior(Directive_Display d)
        : Parametric_Behavior { Directive_Category::pure_html, d, parameters }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, const Argument_Matcher&, Context&)
        const override
    {
    }

    void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override
    {
        std::pmr::vector<char8_t> lang_data { context.get_transient_memory() };
        const std::u8string_view lang
            = argument_to_plaintext_or(lang_data, lang_parameter, u8"", d, args, context);

        const auto mode
            = display == Directive_Display::block ? To_HTML_Mode::trimmed : To_HTML_Mode::direct;
        to_html_syntax_highlighted(out, d.get_content(), lang, context, mode);
    }
};

struct Variable_Behavior : Parametric_Behavior {
    static constexpr std::u8string_view var_parameter = u8"var";
    static constexpr std::u8string_view parameters[] { var_parameter };

    Variable_Behavior(Directive_Category c, Directive_Display d)
        : Parametric_Behavior { c, d, parameters }
    {
    }

    void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override
    {
        std::pmr::vector<char8_t> data { context.get_transient_memory() };
        const std::u8string_view name = get_variable_name(data, d, args, context);
        generate_var_plaintext(out, d, name, context);
    }

    void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override
    {
        std::pmr::vector<char8_t> data { context.get_transient_memory() };
        const std::u8string_view name = get_variable_name(data, d, args, context);
        generate_var_html(out, d, name, context);
    }

protected:
    virtual void generate_var_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const
        = 0;

    virtual void generate_var_html(
        HTML_Writer& out,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const
        = 0;

private:
    static std::u8string_view get_variable_name(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    )
    {
        const int i = args.get_argument_index(var_parameter);
        if (i < 0) {
            // TODO: error when no variable was specified
            return {};
        }
        const ast::Argument& arg = d.get_arguments()[std::size_t(i)];
        // TODO: warn when pure HTML argument was used as variable name
        to_plaintext(out, arg.get_content(), context);
        return { out.data(), out.size() };
    }
};

struct Get_Variable_Behavior final : Variable_Behavior {

    Get_Variable_Behavior()
        : Variable_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void generate_var_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive&,
        std::u8string_view var,
        Context& context
    ) const final
    {
        const auto it = context.get_variables().find(var);
        if (it != context.get_variables().end()) {
            out.insert(out.end(), it->second.begin(), it->second.end());
        }
    }

    void generate_var_html(
        HTML_Writer& out,
        const ast::Directive&,
        std::u8string_view var,
        Context& context
    ) const final
    {
        const auto it = context.get_variables().find(var);
        if (it != context.get_variables().end()) {
            out.write_inner_html(it->second);
        }
    }
};

enum struct Variable_Operation : Default_Underlying {
    // TODO: add more operations
    set
};

[[nodiscard]] [[maybe_unused]]
std::pmr::string vec_to_string(const std::pmr::vector<char>& v)
{
    return { v.data(), v.size(), v.get_allocator() };
}

struct Modify_Variable_Behavior final : Variable_Behavior {
private:
    const Variable_Operation m_op;

public:
    Modify_Variable_Behavior(Variable_Operation op)
        : Variable_Behavior { Directive_Category::meta, Directive_Display::none }
        , m_op { op }
    {
    }

    void process(const ast::Directive& d, std::u8string_view var, Context& context) const
    {
        std::pmr::vector<char8_t> body_string { context.get_transient_memory() };
        to_plaintext(body_string, d.get_content(), context);

        const auto it = context.get_variables().find(var);
        if (m_op == Variable_Operation::set) {
            std::pmr::u8string value { body_string.data(), body_string.size(),
                                       context.get_persistent_memory() };
            if (it == context.get_variables().end()) {
                std::pmr::u8string key // NOLINT(misc-const-correctness)
                    { var.data(), var.size(), context.get_persistent_memory() };
                context.get_variables().emplace(std::move(key), std::move(value));
            }
            else {
                it->second = std::move(value);
            }
        }
    }

    void generate_var_plaintext(
        std::pmr::vector<char8_t>&,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const final
    {
        process(d, var, context);
    }

    void generate_var_html(
        HTML_Writer&,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const final
    {
        process(d, var, context);
    }
};

constexpr std::u8string_view html_tag_prefix = u8"html-";

} // namespace

struct Builtin_Directive_Set::Impl {
    Do_Nothing_Behavior comment { Directive_Category::meta, Directive_Display::none };
    Error_Behavior error {};
    HTML_Literal_Behavior html {};
    Code_Point_Behavior code_point {};
    HTML_Entity_Behavior entity {};
    Syntax_Highlight_Behavior inline_code { Directive_Display::in_line };
    Syntax_Highlight_Behavior code_block { Directive_Display::block };
    Directive_Name_Passthrough_Behavior direct_formatting { Directive_Category::formatting,
                                                            Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior tt_formatting { Directive_Category::formatting,
                                                    Directive_Display::in_line, u8"tt-" };
    Directive_Name_Passthrough_Behavior direct_html { Directive_Category::pure_html,
                                                      Directive_Display::in_line };
    Directive_Name_Passthrough_Behavior html_tags { Directive_Category::pure_html,
                                                    Directive_Display::block, html_tag_prefix };
};

Builtin_Directive_Set::Builtin_Directive_Set()
    : m_impl(std::make_unique<Builtin_Directive_Set::Impl>())
{
}

Builtin_Directive_Set::~Builtin_Directive_Set() = default;

Distant<std::u8string_view> Builtin_Directive_Set::fuzzy_lookup_name(
    std::u8string_view name,
    std::pmr::memory_resource* memory
) const
{
    // clang-format off
    static constexpr std::u8string_view prefixed_names[] {
        u8"-b",
        u8"-c",
        u8"-code",
        u8"-codeblock",
        u8"-comment",
        u8"-dd",
        u8"-dl",
        u8"-dt",
        u8"-em",
        u8"-error",
        u8"-html",
        u8"-html-",
        u8"-i",
        u8"-ins",
        u8"-k",
        u8"-kbd",
        u8"-mark",
        u8"-ol",
        u8"-s",
        u8"-small",
        u8"-strong",
        u8"-sub",
        u8"-sup",
        u8"-tt",
        u8"-U",
        u8"-u",
        u8"-ul",
    };
    // clang-format on
    static_assert(prefixed_names[0][0] == builtin_directive_prefix);

    static constexpr auto all_names = [] {
        std::array<std::u8string_view, std::size(prefixed_names) * 2> result;
        std::ranges::copy(prefixed_names, result.data());
        std::ranges::copy(
            prefixed_names
                | std::views::transform([](std::u8string_view n) { return n.substr(1); }),
            result.data() + std::size(prefixed_names)
        );
        return result;
    }();
    const Distant<std::size_t> result = closest_match(all_names, name, memory);
    if (!result) {
        return {};
    }
    return { .value = all_names[result.value], .distance = result.distance };
}

Directive_Behavior* Builtin_Directive_Set::operator()(std::u8string_view name) const
{
    // Any builtin names should be found with both `\\-directive` and `\\directive`.
    // `\\def` does not permit defining directives with a hyphen prefix,
    // so this lets the user
    if (name.starts_with(builtin_directive_prefix)) {
        return (*this)(name.substr(1));
    }
    if (name.empty()) {
        return nullptr;
    }
    // NOLINTBEGIN(readability-braces-around-statements)
    switch (name[0]) {
    case u8'b':
        if (name == u8"b")
            return &m_impl->direct_formatting;
        break;

    case u8'c':
        if (name == u8"c")
            return &m_impl->code_block;
        if (name == u8"code")
            return &m_impl->inline_code;
        if (name == u8"codeblock")
            return &m_impl->code_block;
        if (name == u8"comment")
            return &m_impl->comment;
        break;

    case u8'd':
        if (name == u8"dd" || name == u8"dl" || name == u8"dt")
            return &m_impl->direct_html;
        break;

    case u8'e':
        if (name == u8"em")
            return &m_impl->direct_formatting;
        if (name == u8"error")
            return &m_impl->error;
        break;

    case u8'h':
        if (name == u8"html")
            return &m_impl->html;
        static_assert(html_tag_prefix[0] == 'h');
        if (name.starts_with(html_tag_prefix))
            return &m_impl->html_tags;
        break;

    case u8'i':
        if (name == u8"i" || name == u8"ins")
            return &m_impl->direct_formatting;
        break;

    case u8'k':
        if (name == u8"kbd")
            return &m_impl->direct_formatting;
        break;

    case u8'm':
        if (name == u8"mark")
            return &m_impl->direct_formatting;
        break;

    case u8'o':
        if (name == u8"ol")
            return &m_impl->direct_html;
        break;

    case u8's':
        if (name == u8"s" || name == u8"small" || name == u8"strong" || name == u8"sub"
            || name == u8"sup")
            return &m_impl->direct_formatting;
        break;

    case u8't':
        if (name == u8"tt")
            return &m_impl->tt_formatting;
        break;

    case u8'U':
        if (name == u8"U")
            return &m_impl->code_point;
        break;

    case u8'u':
        if (name == u8"u")
            return &m_impl->direct_formatting;
        if (name == u8"ul")
            return &m_impl->direct_html;
        break;

    default: break;
    }
    // NOLINTEND(readability-braces-around-statements)

    return nullptr;
}

} // namespace mmml

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory_resource>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mmml/fwd.hpp"
#include "mmml/util/html_writer.hpp"
#include "mmml/util/typo.hpp"

#include "mmml/ast.hpp"
#include "mmml/directive_arguments.hpp"
#include "mmml/directive_processing.hpp"
#include "mmml/directives.hpp"

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
        std::pmr::vector<char8_t> data;
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
        std::pmr::vector<char8_t> data;
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
    Do_Nothing_Behavior do_nothing;
    Error_Behavior error;
    HTML_Literal_Behavior html {};
    Directive_Name_Passthrough_Behavior direct_formatting { Directive_Category::formatting,
                                                            Directive_Display::in_line };
    Fixed_Name_Passthrough_Behavior tt_formatting { Directive_Category::formatting,
                                                    Directive_Display::in_line, u8"tt-" };
    Directive_Name_Passthrough_Behavior direct_html { Directive_Category::pure_html,
                                                      Directive_Display::in_line };
    Directive_Name_Passthrough_Behavior html_tags { Directive_Category::pure_html,
                                                    Directive_Display::block, html_tag_prefix };
};

Builtin_Directive_Set::Builtin_Directive_Set() = default;
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
        u8"-u",
        u8"-ul",
    };
    // clang-format on
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
        if (name == u8"comment")
            return &m_impl->do_nothing;
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

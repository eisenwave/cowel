#ifndef MMML_BUILTIN_DIRECTIVE_SET
#define MMML_BUILTIN_DIRECTIVE_SET

#include <string_view>

#include "mmml/util/html_writer.hpp"

#include "mmml/base_behaviors.hpp"
#include "mmml/directive_behavior.hpp"

namespace mmml {

inline constexpr char8_t builtin_directive_prefix = u8'-';
inline constexpr std::u8string_view html_tag_prefix = u8"html-";

/// @brief Behavior for `\\error` directives.
/// Does not processing.
/// Generates no plaintext.
/// Generates HTML with the source code of the contents wrapped in an `<error->` custom tag.
struct Error_Behavior : Do_Nothing_Behavior {
    static constexpr std::u8string_view id = u8"error-";

    constexpr Error_Behavior()
        : Do_Nothing_Behavior { Directive_Category::pure_html, Directive_Display::in_line }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override
    {
        out.open_tag(id);
        out.write_inner_text(d.get_source(context.get_source()));
        out.close_tag(id);
    }
};

struct [[nodiscard]]
HTML_Entity_Behavior final : Directive_Behavior {
    constexpr HTML_Entity_Behavior()
        : Directive_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override;

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct [[nodiscard]]
Code_Point_Behavior final : Directive_Behavior {

    constexpr Code_Point_Behavior()
        : Directive_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override;

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

// clang-format off
inline constexpr std::u8string_view lorem_ipsum = u8"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
// clang-format on

struct Lorem_Ipsum_Behavior final : Directive_Behavior {
    constexpr Lorem_Ipsum_Behavior()
        : Directive_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive&, Context&) const final
    {
        append(out, lorem_ipsum);
    }

    void generate_html(HTML_Writer& out, const ast::Directive&, Context&) const final
    {
        out.write_inner_html(lorem_ipsum);
    }
};

struct [[nodiscard]] Syntax_Highlight_Behavior : Parametric_Behavior {
private:
    static constexpr std::u8string_view lang_parameter = u8"lang";
    static constexpr std::u8string_view borders_parameter = u8"borders";
    static constexpr std::u8string_view parameters[] { lang_parameter, borders_parameter };

    const std::u8string_view m_tag_name;
    const To_HTML_Mode m_to_html_mode;

public:
    constexpr explicit Syntax_Highlight_Behavior(
        std::u8string_view tag_name,
        Directive_Display d,
        To_HTML_Mode mode
    )
        : Parametric_Behavior { Directive_Category::pure_html, d, parameters }
        , m_tag_name { tag_name }
        , m_to_html_mode { mode }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>&, const ast::Directive&, const Argument_Matcher&, Context&)
        const override;

    void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override;
};

struct HTML_Literal_Behavior : Pure_HTML_Behavior {

    constexpr explicit HTML_Literal_Behavior(Directive_Display display)
        : Pure_HTML_Behavior { display }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override;
};

/// @brief Common behavior for generating `<script>` and `<style>` elements
/// via `\script` and `\style`.
///
/// Note that this behavior is distinct from formatting directives like `\b`.
/// Notably, this produces a pure HTML directive with `block` display.
/// Also, character references (e.g. `&lt;`) have no special meaning in such tags,
/// so the output is not escaped in the usual way but taken quite literally,
/// similar to `HTML_Literal_Behavior`.
struct [[nodiscard]]
HTML_Raw_Text_Behavior final : Pure_HTML_Behavior {
private:
    const std::u8string_view m_tag_name;

public:
    constexpr explicit HTML_Raw_Text_Behavior(std::u8string_view tag_name)
        : Pure_HTML_Behavior { Directive_Display::block }
        , m_tag_name { tag_name }
    {
        MMML_ASSERT(tag_name == u8"style" || tag_name == u8"script");
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override;
};

struct Variable_Behavior : Parametric_Behavior {
    static constexpr std::u8string_view var_parameter = u8"var";
    static constexpr std::u8string_view parameters[] { var_parameter };

    constexpr Variable_Behavior(Directive_Category c, Directive_Display d)
        : Parametric_Behavior { c, d, parameters }
    {
    }

    void generate_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override;

    void generate_html(
        HTML_Writer& out,
        const ast::Directive& d,
        const Argument_Matcher& args,
        Context& context
    ) const override;

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
};

struct Get_Variable_Behavior final : Variable_Behavior {

    constexpr Get_Variable_Behavior()
        : Variable_Behavior { Directive_Category::pure_plaintext, Directive_Display::in_line }
    {
    }

    void generate_var_plaintext(
        std::pmr::vector<char8_t>& out,
        const ast::Directive&,
        std::u8string_view var,
        Context& context
    ) const final;

    void generate_var_html(
        HTML_Writer& out,
        const ast::Directive&,
        std::u8string_view var,
        Context& context
    ) const final;
};

enum struct Variable_Operation : Default_Underlying {
    // TODO: add more operations
    set
};

void process(
    Variable_Operation op,
    const ast::Directive& d,
    std::u8string_view var,
    Context& context
);

struct Modify_Variable_Behavior final : Variable_Behavior {
private:
    const Variable_Operation m_op;

public:
    constexpr Modify_Variable_Behavior(Variable_Operation op)
        : Variable_Behavior { Directive_Category::meta, Directive_Display::none }
        , m_op { op }
    {
    }

    void generate_var_plaintext(
        std::pmr::vector<char8_t>&,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const final
    {
        process(m_op, d, var, context);
    }

    void generate_var_html(
        HTML_Writer&,
        const ast::Directive& d,
        std::u8string_view var,
        Context& context
    ) const final
    {
        process(m_op, d, var, context);
    }
};

struct Wrap_Behavior final : Directive_Behavior {
    constexpr explicit Wrap_Behavior(Directive_Category category, Directive_Display display)
        : Directive_Behavior { category, display }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const final;

    void generate_html(HTML_Writer& out, const ast::Directive&, Context&) const final;
};

struct Passthrough_Behavior : Directive_Behavior {

    constexpr Passthrough_Behavior(Directive_Category category, Directive_Display display)
        : Directive_Behavior { category, display }
    {
    }

    void
    generate_plaintext(std::pmr::vector<char8_t>& out, const ast::Directive& d, Context& context)
        const override;

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override;

    [[nodiscard]]
    virtual std::u8string_view get_name(const ast::Directive& d, Context& context) const
        = 0;
};

struct In_Tag_Behavior : Pure_HTML_Behavior {
private:
    const std::u8string_view m_tag_name;
    const std::u8string_view m_class_name;

public:
    constexpr In_Tag_Behavior(
        std::u8string_view tag_name,
        std::u8string_view class_name,
        Directive_Display display
    )
        : Pure_HTML_Behavior { display }
        , m_tag_name { tag_name }
        , m_class_name { class_name }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const override;
};

/// @brief Behavior for self-closing tags, like `<br/>` and `<hr/>`.
struct Self_Closing_Behavior final : Pure_HTML_Behavior {
private:
    const std::u8string_view m_tag_name;
    const std::u8string_view m_content_ignored_diagnostic;

public:
    constexpr Self_Closing_Behavior(
        std::u8string_view tag_name,
        std::u8string_view content_ignored_diagnostic,
        Directive_Display display
    )
        : Pure_HTML_Behavior { display }
        , m_tag_name { tag_name }
        , m_content_ignored_diagnostic { content_ignored_diagnostic }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

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
    constexpr Directive_Name_Passthrough_Behavior(
        Directive_Category category,
        Directive_Display display,
        const std::u8string_view name_prefix = u8""
    )
        : Passthrough_Behavior { category, display }
        , m_name_prefix { name_prefix }
    {
    }

    [[nodiscard]]
    std::u8string_view get_name(const ast::Directive& d, Context& context) const override;
};

struct Fixed_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const std::u8string_view m_name;

public:
    constexpr explicit Fixed_Name_Passthrough_Behavior(
        std::u8string_view name,
        Directive_Category category,
        Directive_Display display
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

struct Special_Block_Behavior final : Pure_HTML_Behavior {
private:
    const std::u8string_view m_name;
    const bool m_emit_intro;

public:
    constexpr explicit Special_Block_Behavior(std::u8string_view name, bool emit_intro = true)
        : Pure_HTML_Behavior { Directive_Display::block }
        , m_name { name }
        , m_emit_intro { emit_intro }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct WG21_Block_Behavior final : Pure_HTML_Behavior {
private:
    const std::u8string_view m_prefix;
    const std::u8string_view m_suffix;

public:
    constexpr explicit WG21_Block_Behavior(std::u8string_view prefix, std::u8string_view suffix)
        : Pure_HTML_Behavior { Directive_Display::in_line }
        , m_prefix { prefix }
        , m_suffix { suffix }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct WG21_Head_Behavior final : Pure_HTML_Behavior {
    constexpr explicit WG21_Head_Behavior()
        : Pure_HTML_Behavior { Directive_Display::in_line }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct URL_Behavior final : Pure_HTML_Behavior {
private:
    const std::u8string_view m_url_prefix;

public:
    constexpr explicit URL_Behavior(std::u8string_view url_prefix = u8"")
        : Pure_HTML_Behavior { Directive_Display::in_line }
        , m_url_prefix { url_prefix }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct Ref_Behavior final : Pure_HTML_Behavior {

    constexpr explicit Ref_Behavior()
        : Pure_HTML_Behavior(Directive_Display::in_line)
    {
    }

    void generate_html(HTML_Writer&, const ast::Directive& d, Context& context) const final;
};

struct Bibliography_Add_Behavior final : Meta_Behavior {

    void evaluate(const ast::Directive& d, Context& context) const final;
};

struct List_Behavior final : Pure_HTML_Behavior {
private:
    const std::u8string_view m_tag_name;

public:
    constexpr explicit List_Behavior(std::u8string_view tag_name)
        : Pure_HTML_Behavior { Directive_Display::block }
        , m_tag_name { tag_name }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct Heading_Behavior final : Pure_HTML_Behavior {
private:
    const int m_level;

public:
    constexpr Heading_Behavior(int level)
        : Pure_HTML_Behavior { Directive_Display::block }
        , m_level { level }
    {
        MMML_ASSERT(m_level >= 1 && level <= 6);
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct There_Behavior final : Meta_Behavior {

    void evaluate(const ast::Directive& d, Context& context) const final;
};

struct Here_Behavior final : Pure_HTML_Behavior {
    constexpr explicit Here_Behavior(Directive_Display display)
        : Pure_HTML_Behavior { display }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct Make_Section_Behavior final : Pure_HTML_Behavior {
private:
    const std::u8string_view m_class_name;
    const std::u8string_view m_section_name;

public:
    constexpr explicit Make_Section_Behavior(
        Directive_Display display,
        std::u8string_view class_name,
        std::u8string_view section_name
    )
        : Pure_HTML_Behavior { display }
        , m_class_name { class_name }
        , m_section_name { section_name }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct Math_Behavior final : Pure_HTML_Behavior {

    constexpr Math_Behavior(Directive_Display display)
        : Pure_HTML_Behavior { display }
    {
    }

    void generate_html(HTML_Writer& out, const ast::Directive& d, Context& context) const final;
};

struct [[nodiscard]]
Builtin_Directive_Set final : Name_Resolver {
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

public:
    Builtin_Directive_Set();

    Builtin_Directive_Set(const Builtin_Directive_Set&) = delete;
    Builtin_Directive_Set& operator=(const Builtin_Directive_Set&) = delete;

    ~Builtin_Directive_Set();

    [[nodiscard]]
    Directive_Behavior& get_error_behavior() noexcept;

    [[nodiscard]]
    Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, std::pmr::memory_resource* memory) const final;

    [[nodiscard]]
    Directive_Behavior* operator()(std::u8string_view name) const final;
};

namespace class_name {

inline constexpr std::u8string_view bibliography = u8"bib";
inline constexpr std::u8string_view table_of_contents = u8"toc";

} // namespace class_name

namespace section_name {

inline constexpr std::u8string_view bibliography = u8"std.bib";
inline constexpr std::u8string_view document_head = u8"std.head";
inline constexpr std::u8string_view document_body = u8"std.body";
inline constexpr std::u8string_view table_of_contents = u8"std.toc";

} // namespace section_name

} // namespace mmml

#endif

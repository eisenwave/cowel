#ifndef COWEL_BUILTIN_DIRECTIVE_SET
#define COWEL_BUILTIN_DIRECTIVE_SET

#include <memory>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/html_names.hpp"
#include "cowel/util/result.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/context.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/fwd.hpp"
#include "cowel/output_language.hpp"

namespace cowel {

inline constexpr char8_t builtin_directive_prefix = u8'-';
inline constexpr std::u8string_view html_tag_prefix = u8"html-";

/// @brief Indicates which `Content_Policy` should be used by the directive.
enum struct Policy_Usage : bool {
    /// @brief The given policy should be used directly.
    /// This results in applying syntax highlighting and other transformations on content
    /// if the surrounding policy does that.
    inherit,
    /// @brief The content should be processed by an `HTML_Content_Policy`,
    /// which results in the given policy being fed nothing but "HTML blobs".
    /// This is desirable for directives that are not subject to syntax highlighting,
    /// like `\ul`, `\li`, etc.
    html,
};

/// @brief Indicates whether an intro for "special blocks", i.e. notes, examples, etc.
/// should be emitted.
/// That is, a short text snippet at the start of the text content, like "Note:".
enum struct Intro_Policy : bool {
    no,
    yes,
};

struct Deprecated_Behavior final : Directive_Behavior {
private:
    const Directive_Behavior& m_behavior;
    const std::u8string_view m_replacement;

public:
    constexpr Deprecated_Behavior(const Directive_Behavior& other, std::u8string_view replacement)
        : Directive_Behavior { auto(other.get_static_type()) }
        , m_behavior { other }
        , m_replacement { replacement }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status>
    evaluate(const Invocation& call, Context& context) const override
    {
        warn(call, context);
        return m_behavior.evaluate(call, context);
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override
    {
        warn(call, context);
        return m_behavior.splice(out, call, context);
    }

    void warn(const Invocation& call, Context& context) const;
};

/// @brief Behavior for `\\error` directives.
/// Does not processing.
/// Generates HTML with the source code of the contents wrapped in an `<error->` custom tag.
struct Error_Behavior : Block_Directive_Behavior {
    static constexpr auto id = html_tag::error_;

    constexpr explicit Error_Behavior() = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation& call, Context&) const override;
};

struct Comment_Behavior : Unit_Directive_Behavior {
    constexpr explicit Comment_Behavior() = default;

    [[nodiscard]]
    Processing_Status do_evaluate(const Invocation&, Context&) const override
    {
        return Processing_Status::ok;
    }
};

struct [[nodiscard]]
Char_By_Entity_Behavior final : Short_String_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Char_By_Entity_Behavior()
        = default;

    [[nodiscard]]
    Result<Short_String_Value, Processing_Status>
    do_evaluate(const Invocation&, Context&) const override;
};

struct [[nodiscard]] Code_Point_Behavior : Short_String_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Code_Point_Behavior()
        = default;

    [[nodiscard]]
    Result<Short_String_Value, Processing_Status>
    do_evaluate(const Invocation&, Context&) const final;

    virtual Result<char32_t, Processing_Status>
    get_code_point(const Invocation& call, Context& context) const = 0;
};

struct [[nodiscard]]
Char_By_Num_Behavior final : Code_Point_Behavior {
    [[nodiscard]]
    constexpr explicit Char_By_Num_Behavior()
        = default;

    [[nodiscard]]
    Result<char32_t, Processing_Status>
    get_code_point(const Invocation& call, Context& context) const final;
};

struct [[nodiscard]]
Char_By_Name_Behavior final : Code_Point_Behavior {
    [[nodiscard]]
    constexpr explicit Char_By_Name_Behavior()
        = default;

    [[nodiscard]]
    Result<char32_t, Processing_Status>
    get_code_point(const Invocation& call, Context& context) const final;
};

struct [[nodiscard]]
Char_Get_Num_Behavior final : Int_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Char_Get_Num_Behavior()
        = default;

    [[nodiscard]]
    Result<Integer, Processing_Status> do_evaluate(const Invocation&, Context&) const final;
};

struct String_Sink {
    virtual void reserve([[maybe_unused]] std::size_t amount) { }
    virtual void consume(std::pmr::vector<char8_t>&& text) = 0;
    virtual void consume(Char_Sequence8 chars) = 0;
};

struct String_Sink_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit String_Sink_Behavior()
        : Directive_Behavior { Type::str }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation&, Context&) const final;
    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;

protected:
    [[nodiscard]]
    virtual Processing_Status do_evaluate(String_Sink& out, const Invocation&, Context&) const
        = 0;
};

enum struct Text_Transformation : Default_Underlying {
    lowercase,
    uppercase,
};

struct [[nodiscard]]
Str_Transform_Behavior final : String_Sink_Behavior {
private:
    const Text_Transformation m_transform;

public:
    [[nodiscard]]
    constexpr explicit Str_Transform_Behavior(Text_Transformation transform)
        : m_transform { transform }
    {
    }

    [[nodiscard]]
    Processing_Status do_evaluate(String_Sink& out, const Invocation&, Context&) const override;
};

// clang-format off
inline constexpr std::u8string_view lorem_ipsum = u8"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
// clang-format on

struct Lorem_Ipsum_Behavior final : Block_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Lorem_Ipsum_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override
    {
        try_enter_paragraph(out);

        out.write(lorem_ipsum, Output_Language::text);
        return Processing_Status::ok;
    }
};

// TODO: use this for \pre directives too.
enum struct Pre_Trimming : bool { no, yes };

/// @brief Responsible for syntax-highlighted directives like `\code` or `\codeblock`.
struct [[nodiscard]] Code_Behavior : Block_Directive_Behavior {
private:
    const HTML_Tag_Name m_tag_name;
    const Directive_Display m_display;
    const Pre_Trimming m_pre_compat_trim;

public:
    [[nodiscard]]
    constexpr explicit Code_Behavior(
        HTML_Tag_Name tag_name,
        Directive_Display display,
        Pre_Trimming pre_compat_trim
    )
        : m_tag_name { tag_name }
        , m_display { display }
        , m_pre_compat_trim { pre_compat_trim }
    {
    }

    [[nodiscard]]
    Processing_Status splice(Content_Policy&, const Invocation&, Context&) const override;
};

/// @brief Forces a certain highlight to be applied.
struct [[nodiscard]] Highlight_As_Behavior : Block_Directive_Behavior {
private:
    static constexpr std::u8string_view name_parameter = u8"name";
    static constexpr std::u8string_view parameters[] { name_parameter };

public:
    [[nodiscard]]
    constexpr explicit Highlight_As_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
};

enum struct Known_Content_Policy : Default_Underlying {
    current,
    to_html,
    highlight,
    phantom,
    paragraphs,
    no_invoke,
    actions,
    text_only,
    text_as_html,
    source_as_text,
};

struct Policy_Behavior : Block_Directive_Behavior {
private:
    Known_Content_Policy m_policy;

public:
    [[nodiscard]]
    constexpr explicit Policy_Behavior(Known_Content_Policy policy)
        : m_policy { policy }
    {
    }

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
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
HTML_Raw_Text_Behavior final : Block_Directive_Behavior {
private:
    const HTML_Tag_Name m_tag_name;

public:
    [[nodiscard]]
    constexpr explicit HTML_Raw_Text_Behavior(HTML_Tag_Name tag_name)
        : m_tag_name { tag_name }
    {
        COWEL_ASSERT(tag_name == u8"style"sv || tag_name == u8"script"sv);
    }

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
};

struct Logical_Not_Behavior final : Bool_Directive_Behavior {
    [[nodiscard]]
    explicit Logical_Not_Behavior()
        = default;

    [[nodiscard]]
    Result<bool, Processing_Status> do_evaluate(const Invocation&, Context&) const override;
};

enum struct Logical_Expression_Kind : Default_Underlying {
    logical_and,
    logical_or,
};

struct Logical_Expression_Behavior final : Bool_Directive_Behavior {
private:
    const Logical_Expression_Kind m_expression_kind;

public:
    [[nodiscard]]
    constexpr explicit Logical_Expression_Behavior(Logical_Expression_Kind kind)
        : m_expression_kind { kind }
    {
    }

    [[nodiscard]]
    Result<bool, Processing_Status> do_evaluate(const Invocation&, Context&) const override;
};

enum struct Comparison_Expression_Kind : Default_Underlying {
    eq,
    ne,
    lt,
    gt,
    le,
    ge,
};

struct Comparison_Expression_Behavior final : Bool_Directive_Behavior {
private:
    const Comparison_Expression_Kind m_expression_kind;

public:
    [[nodiscard]]
    constexpr explicit Comparison_Expression_Behavior(Comparison_Expression_Kind kind)
        : m_expression_kind { kind }
    {
    }

    [[nodiscard]]
    Result<bool, Processing_Status> do_evaluate(const Invocation&, Context&) const override;
};

enum struct Unary_Numeric_Expression_Kind : Default_Underlying {
    pos,
    neg,
    abs,
    sqrt,
    trunc,
    floor,
    ceil,
    nearest,
    nearest_away_zero,
};

struct Unary_Numeric_Expression_Behavior final : Directive_Behavior {
private:
    const Unary_Numeric_Expression_Kind m_expression_kind;

public:
    [[nodiscard]]
    constexpr explicit Unary_Numeric_Expression_Behavior(Unary_Numeric_Expression_Kind kind)
        : Directive_Behavior { Type::any }
        , m_expression_kind { kind }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation&, Context&) const override;
};

enum struct Integer_Division_Kind : Default_Underlying {
    div_to_zero,
    rem_to_zero,
    div_to_pos_inf,
    rem_to_pos_inf,
    div_to_neg_inf,
    rem_to_neg_inf,
};

struct Integer_Division_Expression_Behavior final : Int_Directive_Behavior {
private:
    const Integer_Division_Kind m_expression_kind;

public:
    [[nodiscard]]
    constexpr explicit Integer_Division_Expression_Behavior(Integer_Division_Kind kind)
        : m_expression_kind { kind }
    {
    }

    [[nodiscard]]
    Result<Integer, Processing_Status> do_evaluate(const Invocation&, Context&) const override;
};

enum struct N_Ary_Numeric_Expression_Kind : Default_Underlying {
    add,
    sub,
    mul,
    div,
    min,
    max,
};

struct N_Ary_Numeric_Expression_Behavior final : Directive_Behavior {
private:
    const N_Ary_Numeric_Expression_Kind m_expression_kind;

public:
    [[nodiscard]]
    constexpr explicit N_Ary_Numeric_Expression_Behavior(N_Ary_Numeric_Expression_Kind kind)
        : Directive_Behavior { Type::any }
        , m_expression_kind { kind }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation&, Context&) const override;
};

struct To_Str_Behavior : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit To_Str_Behavior() noexcept
        : Directive_Behavior { Type::str }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation& call, Context& context) const final;
};

struct Reinterpret_As_Int_Behavior final : Int_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Reinterpret_As_Int_Behavior()
        = default;

    [[nodiscard]]
    Result<Integer, Processing_Status> do_evaluate(const Invocation&, Context&) const override;
};

struct Reinterpret_As_Float_Behavior final : Float_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Reinterpret_As_Float_Behavior()
        = default;

    [[nodiscard]]
    Result<Float, Processing_Status> do_evaluate(const Invocation&, Context&) const override;
};

struct Var_Get_Behavior final : Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Var_Get_Behavior()
        : Directive_Behavior { Type::any }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation&, Context& context) const final;
};

struct Var_Exists_Behavior final : Bool_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Var_Exists_Behavior()
        = default;

    [[nodiscard]]
    Result<bool, Processing_Status> do_evaluate(const Invocation&, Context& context) const final;
};

struct Var_Let_Behavior final : Unit_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Var_Let_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status do_evaluate(const Invocation& call, Context& context) const final;
};

struct Var_Set_Behavior final : Unit_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Var_Set_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status do_evaluate(const Invocation& call, Context& context) const final;
};

struct Var_Delete_Behavior final : Unit_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Var_Delete_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status do_evaluate(const Invocation& call, Context& context) const final;
};

struct Plaintext_Wrapper_Behavior : Block_Directive_Behavior {
protected:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Plaintext_Wrapper_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct Trim_Behavior : Block_Directive_Behavior {
protected:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Trim_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct Passthrough_Behavior : Block_Directive_Behavior {
protected:
    const Policy_Usage m_policy;
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Passthrough_Behavior(Policy_Usage policy, Directive_Display display)
        : m_policy { policy }
        , m_display { display }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;

    [[nodiscard]]
    virtual HTML_Tag_Name get_name(const Invocation& call, Context& context) const
        = 0;
};

enum struct HTML_Element_Self_Closing : bool {
    normal,
    self_closing,
};

struct HTML_Element_Behavior : Block_Directive_Behavior {
private:
    const HTML_Element_Self_Closing m_self_closing;

public:
    [[nodiscard]]
    constexpr explicit HTML_Element_Behavior(HTML_Element_Self_Closing self_closing)
        : m_self_closing { self_closing }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct In_Tag_Behavior : Block_Directive_Behavior {
protected:
    const HTML_Tag_Name m_tag_name;
    const std::u8string_view m_class_name;
    const Policy_Usage m_policy;
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit In_Tag_Behavior(
        HTML_Tag_Name tag_name,
        std::u8string_view class_name,
        Policy_Usage policy,
        Directive_Display display
    )
        : m_tag_name { tag_name }
        , m_class_name { class_name }
        , m_policy { policy }
        , m_display { display }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

/// @brief Behavior for self-closing tags, like `<br/>` and `<hr/>`.
struct Self_Closing_Behavior final : Block_Directive_Behavior {
private:
    const HTML_Tag_Name m_tag_name;
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Self_Closing_Behavior(HTML_Tag_Name tag_name, Directive_Display display)
        : m_tag_name { tag_name }
        , m_display { display }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;
};

struct Fixed_Name_Passthrough_Behavior : Passthrough_Behavior {
private:
    const HTML_Tag_Name m_name;

public:
    [[nodiscard]]
    constexpr explicit Fixed_Name_Passthrough_Behavior(
        HTML_Tag_Name name,
        Policy_Usage policy,
        Directive_Display display
    )
        : Passthrough_Behavior { policy, display }
        , m_name { name }
    {
    }

    [[nodiscard]]
    HTML_Tag_Name get_name(const Invocation&, Context&) const override
    {
        return m_name;
    }
};

struct Special_Block_Behavior final : Block_Directive_Behavior {
private:
    const HTML_Tag_Name m_name;
    const Intro_Policy m_intro;

public:
    [[nodiscard]]
    constexpr explicit Special_Block_Behavior(HTML_Tag_Name name, Intro_Policy intro)
        : m_name { name }
        , m_intro { intro }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;
};

struct WG21_Head_Behavior final : Block_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit WG21_Head_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;
};

struct URL_Behavior final : Block_Directive_Behavior {
private:
    const std::u8string_view m_url_prefix;

public:
    [[nodiscard]]
    constexpr explicit URL_Behavior(std::u8string_view url_prefix = u8"")
        : m_url_prefix { url_prefix }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;
};

struct Ref_Behavior final : Block_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Ref_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy&, const Invocation& call, Context& context) const final;
};

struct Bibliography_Add_Behavior final : Block_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Bibliography_Add_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const final;
};

struct Heading_Behavior final : Block_Directive_Behavior {
private:
    const int m_level;

public:
    [[nodiscard]]
    constexpr explicit Heading_Behavior(int level)
        : m_level { level }
    {
        COWEL_ASSERT(m_level >= 1 && level <= 6);
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct There_Behavior final : Block_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit There_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct Here_Behavior final : Block_Directive_Behavior {
private:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Here_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct Make_Section_Behavior final : Block_Directive_Behavior {
private:
    const Directive_Display m_display;
    const std::u8string_view m_class_name;
    const std::u8string_view m_section_name;

public:
    [[nodiscard]]
    constexpr explicit Make_Section_Behavior(
        Directive_Display display,
        std::u8string_view class_name,
        std::u8string_view section_name
    )
        : m_display { display }
        , m_class_name { class_name }
        , m_section_name { section_name }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct Math_Behavior final : Block_Directive_Behavior {
private:
    const Directive_Display m_display;

public:
    [[nodiscard]]
    constexpr explicit Math_Behavior(Directive_Display display)
        : m_display { display }
    {
    }

    [[nodiscard]]
    Processing_Status
    splice(Content_Policy& out, const Invocation& call, Context& context) const override;
};

struct Include_Text_Behavior final : String_Sink_Behavior {
    [[nodiscard]]
    constexpr explicit Include_Text_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status do_evaluate(String_Sink& out, const Invocation&, Context&) const override;
};

struct Include_Behavior final : Block_Directive_Behavior {
    [[nodiscard]]
    constexpr explicit Include_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
};

struct Macro_Behavior final : Unit_Directive_Behavior {

    [[nodiscard]]
    constexpr explicit Macro_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status do_evaluate(const Invocation&, Context&) const override;
};

struct Put_Behavior final : Directive_Behavior {

    [[nodiscard]]
    constexpr explicit Put_Behavior()
        : Directive_Behavior { auto(Type::any) }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation&, Context&) const override;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;

private:
    [[nodiscard]]
    Result<const ast::Member_Value*, Processing_Status> resolve(const Invocation&, Context&) const;
};

struct Paragraph_Enter_Behavior final : Block_Directive_Behavior {

    constexpr explicit Paragraph_Enter_Behavior() = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
};

struct Paragraph_Leave_Behavior final : Block_Directive_Behavior {

    constexpr explicit Paragraph_Leave_Behavior() = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
};

struct Paragraph_Inherit_Behavior final : Block_Directive_Behavior {

    constexpr explicit Paragraph_Inherit_Behavior() = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
};

struct Invoke_Behavior final : Block_Directive_Behavior {

    [[nodiscard]]
    constexpr explicit Invoke_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status splice(Content_Policy& out, const Invocation&, Context&) const override;
};

struct Alias_Behavior final : Unit_Directive_Behavior {

    [[nodiscard]]
    constexpr explicit Alias_Behavior()
        = default;

    [[nodiscard]]
    Processing_Status do_evaluate(const Invocation&, Context&) const override;
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
    const Directive_Behavior& get_error_behavior() const noexcept;

    [[nodiscard]]
    Distant<std::u8string_view>
    fuzzy_lookup_name(std::u8string_view name, Context& context) const final;

    [[nodiscard]]
    const Directive_Behavior* operator()(std::u8string_view name) const final;
};

namespace class_name {

inline constexpr std::u8string_view bibliography = u8"bib";
inline constexpr std::u8string_view table_of_contents = u8"toc";

} // namespace class_name

namespace section_name {

inline constexpr std::u8string_view bibliography = u8"std.bib";
inline constexpr std::u8string_view id_preview = u8"std.id-preview";
inline constexpr std::u8string_view document_html = u8"std.html";
inline constexpr std::u8string_view document_head = u8"std.head";
inline constexpr std::u8string_view document_body = u8"std.body";
inline constexpr std::u8string_view table_of_contents = u8"std.toc";

} // namespace section_name

} // namespace cowel

#endif

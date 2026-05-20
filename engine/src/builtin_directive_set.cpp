#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/html_names.hpp"
#include "cowel/util/typo.hpp"

#include "cowel/builtin_directive_set.hpp"
#include "cowel/directive_behavior.hpp"
#include "cowel/directive_display.hpp"
#include "cowel/fwd.hpp"

namespace cowel {
namespace {

using namespace std::string_view_literals;

constexpr Internal_Arg_Source_As_Text_Behavior internal_arg_source_as_text //
    {};
constexpr Internal_Eq_Behavior internal_eq //
    {};
constexpr Internal_Expect_Diagnostic_Behavior internal_expect_error //
    { Processing_Status::error, Severity::error };
constexpr Internal_Expect_Diagnostic_Behavior internal_expect_fatal //
    { Processing_Status::fatal, Severity::fatal };
constexpr Internal_Expect_Diagnostic_Behavior internal_expect_warning //
    { Processing_Status::ok, Severity::warning };
constexpr Internal_Typeof_Behavior internal_typeof //
    {};

constexpr Unary_Numeric_Expression_Behavior cowel_abs {
    Unary_Numeric_Expression_Kind::abs,
    u8R"md(### Directive `cowel_abs`
```
cowel_abs(x: int | float): int | float
```
Returns the absolute value of x.

#### Example
`cowel_abs(-7)` → `7`.
)md"sv,
};
constexpr Policy_Behavior cowel_actions {
    Known_Content_Policy::actions,
    u8R"md(### Directive `cowel_actions`
```
cowel_actions(content: block): block
```
Processes content in actions policy; text output is ignored.

#### Example
`cowel_actions{\cowel_macro("m"){...}}` defines a macro without text output.
)md"sv,
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_add {
    N_Ary_Numeric_Expression_Kind::add,
    u8R"md(### Directive `cowel_add`
```
cowel_add(args: pack int | pack float): int | float
```
Returns the sum of all arguments.

#### Example
`cowel_add(1, 2, 3)` → `6`.
)md"sv,
};
constexpr Alias_Behavior cowel_alias {
    u8R"md(### Directive `cowel_alias`
```
cowel_alias(...aliases: str){target: block}: unit
```
Defines aliases for an existing directive.

#### Example
`cowel_alias("N"){cowel_char_by_name}` makes `\N(...)` available.
)md"sv,
};
constexpr Logical_Expression_Behavior cowel_and {
    Logical_Expression_Kind::logical_and,
    u8R"md(### Directive `cowel_and`
```
cowel_and(args: pack lazy bool): bool
```
Logical AND with left-to-right short-circuiting.

#### Example
`cowel_and(true, false)` → `false`.
)md"sv,
};
constexpr Policy_Behavior cowel_as_text {
    Known_Content_Policy::as_text,
    u8R"md(### Directive `cowel_as_text`
```
cowel_as_text(content: block): block
```
Reinterprets generated HTML as plain text.

#### Example
`cowel_as_text{cowel_to_html{x < y}}` renders escaped text.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_bit_not {
    Unary_Numeric_Expression_Kind::bit_not,
    u8R"md(### Directive `cowel_bit_not`
```
cowel_bit_not(x: int): int
```
Returns bitwise NOT of x.

#### Example
`cowel_bit_not(0)` → `-1`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_ceil {
    Unary_Numeric_Expression_Kind::ceil,
    u8R"md(### Directive `cowel_ceil`
```
cowel_ceil(x: float): float
```
Rounds toward positive infinity.

#### Example
`cowel_ceil(1.2)` → `2.0`.
)md"sv,
};
constexpr Char_By_Entity_Behavior cowel_char_by_entity {
    u8R"md(### Directive `cowel_char_by_entity`
```
cowel_char_by_entity(name: str): str
```
Returns a character from an HTML entity name (without `&` and `;`).

#### Example
`cowel_char_by_entity("amp")` → `&`.
)md"sv,
};
constexpr Char_By_Name_Behavior cowel_char_by_name {
    u8R"md(### Directive `cowel_char_by_name`
```
cowel_char_by_name(name: str): str
```
Returns a Unicode code point by official name or alias.

#### Example
`cowel_char_by_name("DIGIT ZERO")` → `0`.
)md"sv,
};
constexpr Char_By_Num_Behavior cowel_char_by_num {
    u8R"md(### Directive `cowel_char_by_num`
```
cowel_char_by_num(num: int): str
```
Returns the Unicode scalar value with numeric code point num.

#### Example
`cowel_char_by_num(0x30)` → `0`.
)md"sv,
};
constexpr Char_Get_Name_Behavior cowel_char_get_name {
    u8R"md(### Directive `cowel_char_get_name`
```
cowel_char_get_name(x: str): str | null
```
Returns the official Unicode name of the first code point in x.

#### Example
`cowel_char_get_name("A")` → `LATIN CAPITAL LETTER A`.
)md"sv,
};
constexpr Char_Get_Num_Behavior cowel_char_get_num {
    u8R"md(### Directive `cowel_char_get_num`
```
cowel_char_get_num(x: str): int
```
Returns numeric value of the first code point in x.

#### Example
`cowel_char_get_num("0")` → `48`.
)md"sv,
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_div {
    N_Ary_Numeric_Expression_Kind::div,
    u8R"md(### Directive `cowel_div`
```
cowel_div(args: pack float): float
```
Divides arguments left to right.

#### Example
`cowel_div(20.0, 5.0, 2.0)` → `2.0`.
)md"sv,
};
constexpr Integer_Division_Expression_Behavior cowel_div_to_neg_inf {
    Integer_Division_Kind::div_to_neg_inf,
    u8R"md(### Directive `cowel_div_to_neg_inf`
```
cowel_div_to_neg_inf(x: int, y: int): int
```
Integer division rounded toward negative infinity.

#### Example
`cowel_div_to_neg_inf(-3, 2)` → `-2`.
)md"sv,
};
constexpr Integer_Division_Expression_Behavior cowel_div_to_pos_inf {
    Integer_Division_Kind::div_to_pos_inf,
    u8R"md(### Directive `cowel_div_to_pos_inf`
```
cowel_div_to_pos_inf(x: int, y: int): int
```
Integer division rounded toward positive infinity.

#### Example
`cowel_div_to_pos_inf(-3, 2)` → `-1`.
)md"sv,
};
constexpr Integer_Division_Expression_Behavior cowel_div_to_zero {
    Integer_Division_Kind::div_to_zero,
    u8R"md(### Directive `cowel_div_to_zero`
```
cowel_div_to_zero(x: int, y: int): int
```
Integer division rounded toward zero.

#### Example
`cowel_div_to_zero(-3, 2)` → `-1`.
)md"sv,
};
constexpr Comparison_Expression_Behavior cowel_eq {
    Comparison_Expression_Kind::eq,
    u8R"md(### Directive `cowel_eq`
```
cowel_eq(x: T, y: T): bool
```
Returns whether two values are equal.

#### Example
`cowel_eq("a", "a")` → `true`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_floor {
    Unary_Numeric_Expression_Kind::floor,
    u8R"md(### Directive `cowel_floor`
```
cowel_floor(x: float): float
```
Rounds toward negative infinity.

#### Example
`cowel_floor(1.9)` → `1.0`.
)md"sv,
};
constexpr Comparison_Expression_Behavior cowel_ge {
    Comparison_Expression_Kind::ge,
    u8R"md(### Directive `cowel_ge`
```
cowel_ge(x: int | float | str, y: int | float | str): bool
```
Returns whether x >= y.

#### Example
`cowel_ge(3, 2)` → `true`.
)md"sv,
};
constexpr Comparison_Expression_Behavior cowel_gt {
    Comparison_Expression_Kind::gt,
    u8R"md(### Directive `cowel_gt`
```
cowel_gt(x: int | float | str, y: int | float | str): bool
```
Returns whether x > y.

#### Example
`cowel_gt(3, 3)` → `false`.
)md"sv,
};
constexpr Highlight_Behavior cowel_highlight {
    u8R"md(### Directive `cowel_highlight`
```
cowel_highlight(lang: str, opaque: bool = false, content: block): block
```
Syntax-highlights content for the given language.

#### Example
`cowel_highlight("cpp"){int x;}` highlights C++ tokens.
)md"sv,
};
constexpr Highlight_As_Behavior cowel_highlight_as {
    u8R"md(### Directive `cowel_highlight_as`
```
cowel_highlight_as(name: str, opaque: bool = false, content: block): block
```
Applies manual highlight category to content.

#### Example
`cowel_highlight_as("keyword-type"){_Int128}` marks a type token.
)md"sv,
};
constexpr Policy_Behavior cowel_highlight_phantom {
    Known_Content_Policy::phantom,
    u8R"md(### Directive `cowel_highlight_phantom`
```
cowel_highlight_phantom(content: block): block
```
Feeds phantom text into highlighting without outputting it.

#### Example
`cowel_highlight_phantom{"{"}` can affect nearby JSON tokenization.
)md"sv,
};
constexpr HTML_Element_Behavior cowel_html_element {
    HTML_Element_Self_Closing::normal,
    u8R"md(### Directive `cowel_html_element`
```
cowel_html_element(name: str, attr: group named str = (), content: block): block
```
Emits an HTML opening and closing tag with content.

#### Example
`cowel_html_element("span", (id="x")){hello}` → `<span id="x">hello</span>`.
)md"sv,
};
constexpr HTML_Element_Behavior cowel_html_self_closing_element {
    HTML_Element_Self_Closing::self_closing,
    u8R"md(### Directive `cowel_html_self_closing_element`
```
cowel_html_self_closing_element(name: str, attr: group named str = (), content: block): block
```
Emits a self-closing HTML element. Content is ignored.

#### Example
`cowel_html_self_closing_element("hr")` → `<hr />`.
)md"sv,
};
constexpr Include_Behavior cowel_include {
    u8R"md(### Directive `cowel_include`
```
cowel_include(path: str): block
```
Loads and processes another COWEL file in current policy.

#### Example
`cowel_include("parts/header.cow")` includes that sub-document.
)md"sv,
};
constexpr Include_Text_Behavior cowel_include_text {
    u8R"md(### Directive `cowel_include_text`
```
cowel_include_text(path: str): str
```
Returns UTF-8 text from a file.

#### Example
`cowel_include_text("example.js")` returns file contents as text.
)md"sv,
};
constexpr Invoke_Behavior cowel_invoke {
    u8R"md(### Directive `cowel_invoke`
```
cowel_invoke(name: str, content: block): any
```
Dynamically invokes a directive by name.

#### Example
`cowel_invoke("cowel_char_by_name"){DIGIT ZERO}` calls it dynamically.
)md"sv,
};
constexpr Comparison_Expression_Behavior cowel_le {
    Comparison_Expression_Kind::le,
    u8R"md(### Directive `cowel_le`
```
cowel_le(x: int | float | str, y: int | float | str): bool
```
Returns whether x <= y.

#### Example
`cowel_le(2, 2)` → `true`.
)md"sv,
};
constexpr Comparison_Expression_Behavior cowel_lt {
    Comparison_Expression_Kind::lt,
    u8R"md(### Directive `cowel_lt`
```
cowel_lt(x: int | float | str, y: int | float | str): bool
```
Returns whether x < y.

#### Example
`cowel_lt("a", "b")` → `true`.
)md"sv,
};
constexpr Macro_Behavior cowel_macro {
    u8R"md(### Directive `cowel_macro`
```
cowel_macro(...names: str, content: block): unit
```
Defines one or more macros.

#### Example
`cowel_macro("m"){Hello}\m` emits `Hello`.
)md"sv,
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_max {
    N_Ary_Numeric_Expression_Kind::max,
    u8R"md(### Directive `cowel_max`
```
cowel_max(args: pack int | pack float): int | float
```
Returns the greatest argument.

#### Example
`cowel_max(3, 9, 4)` → `9`.
)md"sv,
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_min {
    N_Ary_Numeric_Expression_Kind::min,
    u8R"md(### Directive `cowel_min`
```
cowel_min(args: pack int | pack float): int | float
```
Returns the smallest argument.

#### Example
`cowel_min(3, 9, 4)` → `3`.
)md"sv,
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_mul {
    N_Ary_Numeric_Expression_Kind::mul,
    u8R"md(### Directive `cowel_mul`
```
cowel_mul(args: pack int | pack float): int | float
```
Returns product of all arguments.

#### Example
`cowel_mul(2, 3, 4)` → `24`.
)md"sv,
};
constexpr Comparison_Expression_Behavior cowel_ne {
    Comparison_Expression_Kind::ne,
    u8R"md(### Directive `cowel_ne`
```
cowel_ne(x: T, y: T): bool
```
Returns whether two values are not equal.

#### Example
`cowel_ne("a", "b")` → `true`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_nearest {
    Unary_Numeric_Expression_Kind::nearest,
    u8R"md(### Directive `cowel_nearest`
```
cowel_nearest(x: float): float
```
Rounds to nearest integer, ties to even.

#### Example
`cowel_nearest(2.5)` → `2.0`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_nearest_away_zero {
    Unary_Numeric_Expression_Kind::nearest_away_zero,
    u8R"md(### Directive `cowel_nearest_away_zero`
```
cowel_nearest_away_zero(x: float): float
```
Rounds to nearest integer, ties away from zero.

#### Example
`cowel_nearest_away_zero(-2.5)` → `-3.0`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_neg {
    Unary_Numeric_Expression_Kind::neg,
    u8R"md(### Directive `cowel_neg`
```
cowel_neg(x: int | float): int | float
```
Returns arithmetic negation of x.

#### Example
`cowel_neg(7)` → `-7`.
)md"sv,
};
constexpr Policy_Behavior cowel_no_invoke {
    Known_Content_Policy::no_invoke,
    u8R"md(### Directive `cowel_no_invoke`
```
cowel_no_invoke(content: block): block
```
Treats directive invocations as source text.

#### Example
`cowel_no_invoke{\awoo}` outputs literal `\awoo`.
)md"sv,
};
constexpr Logical_Not_Behavior cowel_not {
    u8R"md(### Directive `cowel_not`
```
cowel_not(x: bool): bool
```
Logical NOT.

#### Example
`cowel_not(true)` → `false`.
)md"sv,
};
constexpr Logical_Expression_Behavior cowel_or {
    Logical_Expression_Kind::logical_or,
    u8R"md(### Directive `cowel_or`
```
cowel_or(args: pack lazy bool): bool
```
Logical OR with left-to-right short-circuiting.

#### Example
`cowel_or(false, true)` → `true`.
)md"sv,
};
constexpr Policy_Behavior cowel_paragraphs {
    Known_Content_Policy::paragraphs,
    u8R"md(### Directive `cowel_paragraphs`
```
cowel_paragraphs(content: block): block
```
Processes content with paragraph splitting.

#### Example
`cowel_paragraphs{A\n\nB}` emits two paragraphs.
)md"sv,
};
constexpr Paragraph_Enter_Behavior cowel_paragraph_enter {
    u8R"md(### Directive `cowel_paragraph_enter`
```
cowel_paragraph_enter(): block
```
In paragraphs policy, opens a paragraph if currently outside one.

#### Example
Use before text to force paragraph start.
)md"sv,
};
constexpr Paragraph_Inherit_Behavior cowel_paragraph_inherit {
    u8R"md(### Directive `cowel_paragraph_inherit`
```
cowel_paragraph_inherit(): block
```
Marks generated content as inheriting surrounding paragraph splitting.

#### Example
Used in programmatic directives to emulate macro/include inheritance.
)md"sv,
};
constexpr Paragraph_Leave_Behavior cowel_paragraph_leave {
    u8R"md(### Directive `cowel_paragraph_leave`
```
cowel_paragraph_leave(): block
```
In paragraphs policy, closes current paragraph if one is open.

#### Example
Often used before block-only HTML like `hr`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_pos {
    Unary_Numeric_Expression_Kind::pos,
    u8R"md(### Directive `cowel_pos`
```
cowel_pos(x: int | float): int | float
```
Returns x unchanged.

#### Example
`cowel_pos(-4)` → `-4`.
)md"sv,
};
constexpr Put_Behavior cowel_put {
    u8R"md(### Directive `cowel_put`
```
cowel_put(selector?: str | int, else?: any): any
```
In macro expansion, inserts supplied content or arguments.

#### Example
If `m` is `cowel_macro("m"){cowel_put{0}}`, then `\m("x")` emits `x`.
)md"sv,
};
constexpr Regex_Make_Behavior cowel_regex {
    u8R"md(### Directive `cowel_regex`
```
cowel_regex(pattern: str): regex
```
Compiles a regular-expression value.

#### Example
`cowel_str_match("awoo", cowel_regex("awo+"))` → `true`.
)md"sv,
};
constexpr Reinterpret_As_Float_Behavior cowel_reinterpret_as_float {
    u8R"md(### Directive `cowel_reinterpret_as_float`
```
cowel_reinterpret_as_float(x: int): float
```
Reinterprets integer bits as binary64 float.

#### Example
`cowel_reinterpret_as_float(0x3ff8000000000000)` → `1.5`.
)md"sv,
};
constexpr Reinterpret_As_Int_Behavior cowel_reinterpret_as_int {
    u8R"md(### Directive `cowel_reinterpret_as_int`
```
cowel_reinterpret_as_int(x: float): int
```
Reinterprets float bits as integer.

#### Example
`cowel_reinterpret_as_int(1.5)` → `0x3ff8000000000000`.
)md"sv,
};
constexpr Integer_Division_Expression_Behavior cowel_rem_to_neg_inf {
    Integer_Division_Kind::rem_to_neg_inf,
    u8R"md(### Directive `cowel_rem_to_neg_inf`
```
cowel_rem_to_neg_inf(x: int, y: int): int
```
Remainder with floor-division semantics.

#### Example
`cowel_rem_to_neg_inf(-3, 2)` → `1`.
)md"sv,
};
constexpr Integer_Division_Expression_Behavior cowel_rem_to_pos_inf {
    Integer_Division_Kind::rem_to_pos_inf,
    u8R"md(### Directive `cowel_rem_to_pos_inf`
```
cowel_rem_to_pos_inf(x: int, y: int): int
```
Remainder with ceil-division semantics.

#### Example
`cowel_rem_to_pos_inf(-3, 2)` → `-1`.
)md"sv,
};
constexpr Integer_Division_Expression_Behavior cowel_rem_to_zero {
    Integer_Division_Kind::rem_to_zero,
    u8R"md(### Directive `cowel_rem_to_zero`
```
cowel_rem_to_zero(x: int, y: int): int
```
Remainder with truncating division semantics.

#### Example
`cowel_rem_to_zero(-3, 2)` → `-1`.
)md"sv,
};
constexpr Policy_Behavior cowel_source_as_text {
    Known_Content_Policy::source_as_text,
    u8R"md(### Directive `cowel_source_as_text`
```
cowel_source_as_text(content: block): block
```
Treats content as literal COWEL source text.

#### Example
`cowel_source_as_text{\: comment}` emits literal `\: comment`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_sqrt {
    Unary_Numeric_Expression_Kind::sqrt,
    u8R"md(### Directive `cowel_sqrt`
```
cowel_sqrt(x: float): float
```
Returns square root of x.

#### Example
`cowel_sqrt(9.0)` → `3.0`.
)md"sv,
};
constexpr Str_Contains_Behavior cowel_str_contains {
    u8R"md(### Directive `cowel_str_contains`
```
cowel_str_contains(text: str, needle: str | regex): bool
```
Returns whether needle occurs in text.

#### Example
`cowel_str_contains("awoo", "o")` → `true`.
)md"sv,
};
constexpr Str_Find_Behavior cowel_str_find {
    u8R"md(### Directive `cowel_str_find`
```
cowel_str_find(text: str, needle: str | regex): int | null
```
Returns first match index in code points, or null.

#### Example
`cowel_str_find("awoo", "o")` → `2`.
)md"sv,
};
constexpr Str_Substr_Behavior cowel_str_substr {
    u8R"md(### Directive `cowel_str_substr`
```
cowel_str_substr(text: str, start: int, length: int | null = null): str
```
Extracts substring by code-point index.

#### Example
`cowel_str_substr("awoo", 1, 2)` → `wo`.
)md"sv,
};
constexpr Str_Length_Behavior cowel_str_length {
    Str_Length_Kind::code_point,
    u8R"md(### Directive `cowel_str_length`
```
cowel_str_length(x: str): int
```
Returns length in Unicode code points.

#### Example
`cowel_str_length("a")` → `1`.
)md"sv,
};
constexpr Str_Match_Behavior cowel_str_match {
    u8R"md(### Directive `cowel_str_match`
```
cowel_str_match(text: str, regex: regex): bool
```
Returns whether the whole string matches regex.

#### Example
`cowel_str_match("awoo", cowel_regex("awo+"))` → `true`.
)md"sv,
};
constexpr Str_Replace_Behavior cowel_str_replace_all {
    Str_Replacement_Kind::all,
    u8R"md(### Directive `cowel_str_replace_all`
```
cowel_str_replace_all(text: str, needle: str | regex, with: str): str
```
Replaces all matches in text.

#### Example
`cowel_str_replace_all("awoo", "o", "x")` → `awxx`.
)md"sv,
};
constexpr Str_Replace_Behavior cowel_str_replace_first {
    Str_Replacement_Kind::first,
    u8R"md(### Directive `cowel_str_replace_first`
```
cowel_str_replace_first(text: str, needle: str | regex, with: str): str
```
Replaces first match in text.

#### Example
`cowel_str_replace_first("awoo", "o", "x")` → `awxo`.
)md"sv,
};
constexpr Str_Transform_Behavior cowel_str_to_lower {
    Text_Transformation::lowercase,
    u8R"md(### Directive `cowel_str_to_lower`
```
cowel_str_to_lower(x: str): str
```
Converts to lowercase with Unicode default mapping.

#### Example
`cowel_str_to_lower("Awoo")` → `awoo`.
)md"sv,
};
constexpr Str_Transform_Behavior cowel_str_to_upper {
    Text_Transformation::uppercase,
    u8R"md(### Directive `cowel_str_to_upper`
```
cowel_str_to_upper(x: str): str
```
Converts to uppercase with Unicode default mapping.

#### Example
`cowel_str_to_upper("Awoo")` → `AWOO`.
)md"sv,
};
constexpr Str_Length_Behavior cowel_str_utf8_length {
    Str_Length_Kind::utf8,
    u8R"md(### Directive `cowel_str_utf8_length`
```
cowel_str_utf8_length(x: str): int
```
Returns length in UTF-8 code units.

#### Example
`cowel_str_utf8_length("a")` → `1`.
)md"sv,
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_sub {
    N_Ary_Numeric_Expression_Kind::sub,
    u8R"md(### Directive `cowel_sub`
```
cowel_sub(args: pack int | pack float): int | float
```
Subtracts arguments left to right.

#### Example
`cowel_sub(10, 3, 2)` → `5`.
)md"sv,
};
constexpr Policy_Behavior cowel_text_as_html {
    Known_Content_Policy::text_as_html,
    u8R"md(### Directive `cowel_text_as_html`
```
cowel_text_as_html(content: block): block
```
Treats text as HTML markup.

#### Example
`cowel_text_as_html{<strong>x</strong>}` emits HTML tags.
)md"sv,
};
constexpr Policy_Behavior cowel_text_only {
    Known_Content_Policy::text_only,
    u8R"md(### Directive `cowel_text_only`
```
cowel_text_only(content: block): block
```
Keeps plaintext and strips HTML output.

#### Example
`cowel_text_only{Hello, \cowel_html_element("strong"){x}}` yields plain text.
)md"sv,
};
constexpr Policy_Behavior cowel_to_html {
    Known_Content_Policy::to_html,
    u8R"md(### Directive `cowel_to_html`
```
cowel_to_html(content: block): block
```
Processes content with to-HTML policy.

#### Example
`cowel_to_html{x < y}` emits HTML-safe output.
)md"sv,
};
constexpr To_Str_Behavior cowel_to_str {
    u8R"md(### Directive `cowel_to_str`
```
cowel_to_str(x: any, ...): str
```
Converts a value to string, with numeric formatting options.

#### Example
`cowel_to_str(255, base=16)` → `ff`.
)md"sv,
};
constexpr Unary_Numeric_Expression_Behavior cowel_trunc {
    Unary_Numeric_Expression_Kind::trunc,
    u8R"md(### Directive `cowel_trunc`
```
cowel_trunc(x: float): float
```
Rounds toward zero.

#### Example
`cowel_trunc(-1.9)` → `-1.0`.
)md"sv,
};
constexpr Var_Delete_Behavior cowel_var_delete {
    u8R"md(### Directive `cowel_var_delete`
```
cowel_var_delete(name: str): unit
```
Deletes a variable binding.

#### Example
`cowel_var_delete("x")` removes x.
)md"sv,
};
constexpr Var_Exists_Behavior cowel_var_exists {
    u8R"md(### Directive `cowel_var_exists`
```
cowel_var_exists(name: str): bool
```
Returns whether a variable exists.

#### Example
`cowel_var_exists("x")` → `true` if x is defined.
)md"sv,
};
constexpr Var_Get_Behavior cowel_var_get {
    u8R"md(### Directive `cowel_var_get`
```
cowel_var_get(name: str): any
```
Returns value of a variable.

#### Example
After `cowel_var_let("x", 3)`, `cowel_var_get("x")` → `3`.
)md"sv,
};
constexpr Var_Let_Behavior cowel_var_let {
    u8R"md(### Directive `cowel_var_let`
```
cowel_var_let(name: str, value: any): unit
```
Defines a new variable.

#### Example
`cowel_var_let("x", 3)` creates x.
)md"sv,
};
constexpr Var_Set_Behavior cowel_var_set {
    u8R"md(### Directive `cowel_var_set`
```
cowel_var_set(name: str, value: any): unit
```
Updates an existing variable.

#### Example
After `cowel_var_let("x", 1)`, `cowel_var_set("x", 2)` updates x.
)md"sv,
};

// Legacy directives
constexpr Fixed_Name_Passthrough_Behavior b {
    html_tag::b,
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `b`
```
b(
  attr: pack named str,
  content: block,
): block
```
Makes content bold.

#### Example
`b{hello}` → `<b>hello</b>`
)md"sv,
};
constexpr Special_Block_Behavior Babstract {
    HTML_Tag_Name(u8"abstract-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Babstract`
```
Babstract(attr: pack named str, content: block): block
```
Creates an `<abstract-block>` callout with an intro label.
)md"sv,
};
constexpr Special_Block_Behavior Bdecision {
    HTML_Tag_Name(u8"decision-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Bdecision`
```
Bdecision(attr: pack named str, content: block): block
```
Creates a `<decision-block>` callout with an intro label.
)md"sv,
};
constexpr Special_Block_Behavior Bdel {
    HTML_Tag_Name(u8"del-block"),
    Intro_Policy::no,
    u8R"md(### Directive `Bdel`
```
Bdel(attr: pack named str, content: block): block
```
Creates a `<del-block>` block for marking deleted content.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior Bdetails {
    HTML_Tag_Name(u8"details"),
    Policy_Usage::inherit,
    Directive_Display::block,
    u8R"md(### Directive `Bdetails`
```
Bdetails(attr: pack named str, content: block): block
```
Wraps content in an HTML `<details>` disclosure block element.
)md"sv,
};
constexpr Special_Block_Behavior Bdiff {
    HTML_Tag_Name(u8"diff-block"),
    Intro_Policy::no,
    u8R"md(### Directive `Bdiff`
```
Bdiff(attr: pack named str, content: block): block
```
Creates a `<diff-block>` block for showing diff content.
)md"sv,
};
constexpr Special_Block_Behavior Bex {
    HTML_Tag_Name(u8"example-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Bex`
```
Bex(attr: pack named str, content: block): block
```
Creates an `<example-block>` callout with an intro label.
)md"sv,
};
constexpr Bibliography_Add_Behavior bib {
    u8R"md(### Directive `bib`
```
bib(id: str, title: str, date: str, publisher: str, link: str, long_link: str, author: str): block
```
Adds a bibliography entry.
`id` is mandatory; all other fields are optional.
The entry can be referenced with `\ref`.

#### Example
`bib(id: ISO23, title: C++23 Standard, date: 2024)` registers a bibliography entry.
)md"sv,
};
constexpr Special_Block_Behavior Bimp {
    HTML_Tag_Name(u8"important-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Bimp`
```
Bimp(attr: pack named str, content: block): block
```
Creates an `<important-block>` callout with an intro label.
)md"sv,
};
constexpr In_Tag_Behavior Bindent {
    html_tag::div,
    u8"indent",
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `Bindent`
```
Bindent(attr: pack named str, content: block): block
```
Wraps content in a `<div class="indent">` for indented block content.
)md"sv,
};
constexpr Special_Block_Behavior Bins {
    HTML_Tag_Name(u8"ins-block"),
    Intro_Policy::no,
    u8R"md(### Directive `Bins`
```
Bins(attr: pack named str, content: block): block
```
Creates an `<ins-block>` block for marking inserted content.
)md"sv,
};
constexpr Special_Block_Behavior blockquote {
    HTML_Tag_Name(u8"blockquote"),
    Intro_Policy::no,
    u8R"md(### Directive `blockquote`
```
blockquote(attr: pack named str, content: block): block
```
Creates an HTML `<blockquote>` element.
)md"sv,
};
constexpr Special_Block_Behavior Bnote {
    HTML_Tag_Name(u8"note-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Bnote`
```
Bnote(attr: pack named str, content: block): block
```
Creates a `<note-block>` callout with an intro label.
)md"sv,
};
constexpr Special_Block_Behavior Bquote {
    HTML_Tag_Name(u8"blockquote"),
    Intro_Policy::no,
    u8R"md(### Directive `Bquote`
```
Bquote(attr: pack named str, content: block): block
```
Creates an HTML `<blockquote>` element.
Alias of `blockquote`.
)md"sv,
};
constexpr Self_Closing_Behavior br {
    html_tag::br,
    Directive_Display::in_line,
    u8R"md(### Directive `br`
```
br(): block
```
Inserts an HTML `<br>` line break.

#### Example
`br` or `br{}` → `<br>`
)md"sv,
};
constexpr Special_Block_Behavior Btip {
    HTML_Tag_Name(u8"tip-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Btip`
```
Btip(attr: pack named str, content: block): block
```
Creates a `<tip-block>` callout with an intro label.
)md"sv,
};
constexpr Special_Block_Behavior Btodo {
    HTML_Tag_Name(u8"todo-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Btodo`
```
Btodo(attr: pack named str, content: block): block
```
Creates a `<todo-block>` callout with an intro label.
)md"sv,
};
constexpr Special_Block_Behavior Bug {
    HTML_Tag_Name(u8"bug-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Bug`
```
Bug(attr: pack named str, content: block): block
```
Creates a `<bug-block>` callout with an intro label.
)md"sv,
};
constexpr Special_Block_Behavior Bwarn {
    HTML_Tag_Name(u8"warning-block"),
    Intro_Policy::yes,
    u8R"md(### Directive `Bwarn`
```
Bwarn(attr: pack named str, content: block): block
```
Creates a `<warning-block>` callout with an intro label.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior caption {
    HTML_Tag_Name(u8"caption"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `caption`
```
caption(attr: pack named str, content: block): block
```
Wraps content in an HTML `<caption>` element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior cite {
    HTML_Tag_Name(u8"cite"),
    Policy_Usage::html,
    Directive_Display::in_line,
    u8R"md(### Directive `cite`
```
cite(attr: pack named str, content: block): block
```
Wraps content in an HTML `<cite>` citation element.

#### Example
`cite{My Book}` → `<cite>My Book</cite>`
)md"sv,
};
constexpr Code_Behavior code {
    HTML_Tag_Name(u8"code"),
    Directive_Display::in_line,
    Pre_Trimming::no,
    u8R"md(### Directive `code`
```
code(lang: str, content: block): block
```
Renders content as syntax-highlighted inline code in a `<code>` element.

#### Example
`code("cpp"){int}` → `<code><h- data-h=kw>int</h-></code>`
)md"sv,
};
constexpr Code_Behavior codeblock {
    HTML_Tag_Name(u8"code-block"),
    Directive_Display::block,
    Pre_Trimming::yes,
    u8R"md(### Directive `codeblock`
```
codeblock(lang: str, content: block): block
```
Renders content as a syntax-highlighted code block in a `<code-block>` element.

#### Example
`codeblock("cpp"){int main() {}}` produces a highlighted block.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior col {
    HTML_Tag_Name(u8"col"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `col`
```
col(attr: pack named str, content: block): block
```
Wraps content in an HTML `<col>` table column element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior colgroup {
    HTML_Tag_Name(u8"colgroup"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `colgroup`
```
colgroup(attr: pack named str, content: block): block
```
Wraps content in an HTML `<colgroup>` table column group element.
)md"sv,
};
constexpr Comment_Behavior comment {
    u8R"md(### Directive `comment`
```
comment(content: block): unit
```
Discards its content — acts as a document comment.

#### Example
`comment{This text will not appear in output.}` emits nothing.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior dd {
    HTML_Tag_Name(u8"dd"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `dd`
```
dd(attr: pack named str, content: block): block
```
Wraps content in an HTML `<dd>` description definition element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior del {
    HTML_Tag_Name(u8"del"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `del`
```
del(attr: pack named str, content: block): block
```
Wraps content in an HTML `<del>` element for deleted text.

#### Example
`del{old text}` → `<del>old text</del>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior details {
    HTML_Tag_Name(u8"details"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `details`
```
details(attr: pack named str, content: block): block
```
Wraps content in an HTML `<details>` disclosure element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior dfn {
    HTML_Tag_Name(u8"dfn"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `dfn`
```
dfn(attr: pack named str, content: block): block
```
Wraps content in an HTML `<dfn>` definition term element.

#### Example
`dfn{iterator}` → `<dfn>iterator</dfn>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior div {
    html_tag::div,
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `div`
```
div(attr: pack named str, content: block): block
```
Wraps content in an HTML `<div>` block element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior dl {
    HTML_Tag_Name(u8"dl"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `dl`
```
dl(attr: pack named str, content: block): block
```
Wraps content in an HTML `<dl>` description list element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior dt {
    HTML_Tag_Name(u8"dt"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `dt`
```
dt(attr: pack named str, content: block): block
```
Wraps content in an HTML `<dt>` description term element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior em {
    HTML_Tag_Name(u8"em"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `em`
```
em(attr: pack named str, content: block): block
```
Wraps content in an HTML `<em>` emphasis element.

#### Example
`em{hello}` → `<em>hello</em>`
)md"sv,
};
constexpr Error_Behavior error {
    u8R"md(### Directive `error`
```
error(content: block): block
```
Renders content as a visible error in an `<error->` element.
Typically used to display error output inline.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior gterm {
    HTML_Tag_Name(u8"g-term"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `gterm`
```
gterm(attr: pack named str, content: block): block
```
Wraps content in a `<g-term>` grammar term element.
)md"sv,
};
constexpr Heading_Behavior h1 {
    1,
    u8R"md(### Directive `h1`
```
h1(id: str, attr: pack named str, content: block): block
```
Creates a level 1 `<h1>` heading.
Headings are automatically numbered and added to the table of contents.

#### Example
`h1{Introduction}` → `<h1>Introduction</h1>`
)md"sv,
};
constexpr Heading_Behavior h2 {
    2,
    u8R"md(### Directive `h2`
```
h2(id: str, attr: pack named str, content: block): block
```
Creates a level 2 `<h2>` heading.
Headings are automatically numbered and added to the table of contents.
)md"sv,
};
constexpr Heading_Behavior h3 {
    3,
    u8R"md(### Directive `h3`
```
h3(id: str, attr: pack named str, content: block): block
```
Creates a level 3 `<h3>` heading.
Headings are automatically numbered and added to the table of contents.
)md"sv,
};
constexpr Heading_Behavior h4 {
    4,
    u8R"md(### Directive `h4`
```
h4(id: str, attr: pack named str, content: block): block
```
Creates a level 4 `<h4>` heading.
Headings are automatically numbered and added to the table of contents.
)md"sv,
};
constexpr Heading_Behavior h5 {
    5,
    u8R"md(### Directive `h5`
```
h5(id: str, attr: pack named str, content: block): block
```
Creates a level 5 `<h5>` heading.
Headings are automatically numbered and added to the table of contents.
)md"sv,
};
constexpr Heading_Behavior h6 {
    6,
    u8R"md(### Directive `h6`
```
h6(id: str, attr: pack named str, content: block): block
```
Creates a level 6 `<h6>` heading.
Headings are automatically numbered and added to the table of contents.
)md"sv,
};
constexpr Here_Behavior here {
    Directive_Display::in_line,
    u8R"md(### Directive `here`
```
here(section: str, content: block): block
```
Inserts previously stashed section content inline at this point.
Use `there` to stash content for a named section.
)md"sv,
};
constexpr Here_Behavior hereblock {
    Directive_Display::block,
    u8R"md(### Directive `hereblock`
```
hereblock(section: str, content: block): block
```
Inserts previously stashed section content as a block at this point.
Use `there` to stash content for a named section.
)md"sv,
};
constexpr Self_Closing_Behavior hr {
    HTML_Tag_Name(u8"hr"),
    Directive_Display::block,
    u8R"md(### Directive `hr`
```
hr(): block
```
Inserts an HTML `<hr>` horizontal rule.

#### Example
`hr` or `hr{}` → `<hr>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior i {
    HTML_Tag_Name(u8"i"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `i`
```
i(attr: pack named str, content: block): block
```
Wraps content in an HTML `<i>` italic element.

#### Example
`i{hello}` → `<i>hello</i>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior ins {
    HTML_Tag_Name(u8"ins"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `ins`
```
ins(attr: pack named str, content: block): block
```
Wraps content in an HTML `<ins>` element for inserted text.

#### Example
`ins{new text}` → `<ins>new text</ins>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior kbd {
    HTML_Tag_Name(u8"kbd"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `kbd`
```
kbd(attr: pack named str, content: block): block
```
Wraps content in an HTML `<kbd>` keyboard input element.

#### Example
`kbd{Ctrl+C}` → `<kbd>Ctrl+C</kbd>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior li {
    HTML_Tag_Name(u8"li"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `li`
```
li(attr: pack named str, content: block): block
```
Wraps content in an HTML `<li>` list item element.
)md"sv,
};
constexpr Lorem_Ipsum_Behavior lorem_ipsum {
    u8R"md(### Directive `lorem_ipsum`
```
lorem_ipsum(): block
```
Outputs a standard Lorem Ipsum placeholder paragraph.

#### Example
`lorem_ipsum` emits a paragraph of Lorem Ipsum text.
)md"sv,
};
constexpr URL_Behavior mail {
    u8"mailto:",
    u8R"md(### Directive `mail`
```
mail(content: block): block
```
Creates a `mailto:` hyperlink where the content is the email address.

#### Example
`mail{user@example.com}` → `<a href="mailto:user@example.com">user@example.com</a>`
)md"sv,
};
constexpr Make_Section_Behavior make_bib {
    Directive_Display::block,
    class_name::bibliography,
    section_name::bibliography,
    u8R"md(### Directive `make_bib`
```
make_bib(): block
```
Inserts the bibliography section at this location.
Bibliography entries must be added with `bib` beforehand.
)md"sv,
};
constexpr Make_Section_Behavior make_contents {
    Directive_Display::block,
    class_name::table_of_contents,
    section_name::table_of_contents,
    u8R"md(### Directive `make_contents`
```
make_contents(): block
```
Inserts the table of contents at this location.
The table of contents is built from headings throughout the document.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior mark {
    HTML_Tag_Name(u8"mark"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `mark`
```
mark(attr: pack named str, content: block): block
```
Wraps content in an HTML `<mark>` highlight element.

#### Example
`mark{important}` → `<mark>important</mark>`
)md"sv,
};
constexpr Math_Behavior math {
    Directive_Display::in_line,
    u8R"md(### Directive `math`
```
math(attr: pack named str, content: block): block
```
Renders content as an inline math expression (MathML).

#### Example
`math{x^2 + y^2}` produces inline math.
)md"sv,
};
constexpr Math_Behavior mathblock {
    Directive_Display::block,
    u8R"md(### Directive `mathblock`
```
mathblock(attr: pack named str, content: block): block
```
Renders content as a block-level math expression (MathML).
)md"sv,
};
constexpr In_Tag_Behavior nobr {
    html_tag::span,
    u8"word",
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `nobr`
```
nobr(attr: pack named str, content: block): block
```
Wraps content in a `<span class="word">` to prevent line breaks within the content.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior noscript {
    HTML_Tag_Name(u8"noscript"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `noscript`
```
noscript(attr: pack named str, content: block): block
```
Wraps content in an HTML `<noscript>` element,
shown when JavaScript is disabled.
)md"sv,
};
constexpr In_Tag_Behavior o {
    html_tag::span,           u8"oblique", Policy_Usage::inherit, Directive_Display::in_line,
    u8R"md(### Directive `o`
```
o(attr: pack named str, content: block): block
```
Wraps content in a `<span class="oblique">` for oblique styling.

#### Example
`o{hello}` → `<span class="oblique">hello</span>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior ol {
    html_tag::ol,
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `ol`
```
ol(attr: pack named str, content: block): block
```
Wraps content in an HTML `<ol>` ordered list element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior p {
    HTML_Tag_Name(u8"p"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `p`
```
p(attr: pack named str, content: block): block
```
Wraps content in an HTML `<p>` paragraph element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior pre {
    HTML_Tag_Name(u8"pre"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `pre`
```
pre(attr: pack named str, content: block): block
```
Wraps content in an HTML `<pre>` preformatted text element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior q {
    HTML_Tag_Name(u8"q"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `q`
```
q(attr: pack named str, content: block): block
```
Wraps content in an HTML `<q>` inline quotation element.

#### Example
`q{hello}` → `<q>hello</q>`
)md"sv,
};
constexpr Ref_Behavior ref {
    u8R"md(### Directive `ref`
```
ref(
  to: str,
  content: block | null = null,
): block
```
Creates a hyperlink or document reference.
`to` is the target URL or anchor;
`content` is the visible link content (auto-generated if not provided).

#### Example
`ref("#sec1"){See section 1}` → `<a href="#sec1">See section 1</a>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior s {
    HTML_Tag_Name(u8"s"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `s`
```
s(attr: pack named str, content: block): block
```
Wraps content in an HTML `<s>` strikethrough element.

#### Example
`s{wrong}` → `<s>wrong</s>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior samp {
    HTML_Tag_Name(u8"samp"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `samp`
```
samp(attr: pack named str, content: block): block
```
Wraps content in an HTML `<samp>` sample output element.

#### Example
`samp{error: file not found}` → `<samp>error: file not found</samp>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior sans {
    HTML_Tag_Name(u8"f-sans"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `sans`
```
sans(attr: pack named str, content: block): block
```
Wraps content in an `<f-sans>` element for sans-serif styling.

#### Example
`sans{hello}` → `<f-sans>hello</f-sans>`
)md"sv,
};
constexpr HTML_Raw_Text_Behavior script {
    html_tag::script,
    u8R"md(### Directive `script`
```
script(attr: pack named str, content: block): block
```
Embeds raw content as an HTML `<script>` element.
Content is taken literally (not HTML-escaped).
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior serif {
    HTML_Tag_Name(u8"f-serif"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `serif`
```
serif(attr: pack named str, content: block): block
```
Wraps content in an `<f-serif>` element for serif styling.

#### Example
`serif{hello}` → `<f-serif>hello</f-serif>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior small {
    HTML_Tag_Name(u8"small"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `small`
```
small(attr: pack named str, content: block): block
```
Wraps content in an HTML `<small>` element for smaller text.

#### Example
`small{note}` → `<small>note</small>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior span {
    html_tag::span,
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `span`
```
span(attr: pack named str, content: block): block
```
Wraps content in an HTML `<span>` inline container element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior strong {
    HTML_Tag_Name(u8"strong"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `strong`
```
strong(attr: pack named str, content: block): block
```
Wraps content in an HTML `<strong>` element for strong emphasis.

#### Example
`strong{important}` → `<strong>important</strong>`
)md"sv,
};
constexpr HTML_Raw_Text_Behavior style {
    html_tag::style,
    u8R"md(### Directive `style`
```
style(attr: pack named str, content: block): block
```
Embeds raw content as an HTML `<style>` element.
Content is taken literally (not HTML-escaped).
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior sub {
    HTML_Tag_Name(u8"sub"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `sub`
```
sub(attr: pack named str, content: block): block
```
Wraps content in an HTML `<sub>` subscript element.

#### Example
`sub{2}` → `<sub>2</sub>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior summary {
    HTML_Tag_Name(u8"summary"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `summary`
```
summary(attr: pack named str, content: block): block
```
Wraps content in an HTML `<summary>` element, used inside `details`.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior sup {
    HTML_Tag_Name(u8"sup"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `sup`
```
sup(attr: pack named str, content: block): block
```
Wraps content in an HTML `<sup>` superscript element.

#### Example
`sup{2}` → `<sup>2</sup>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior table {
    HTML_Tag_Name(u8"table"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `table`
```
table(attr: pack named str, content: block): block
```
Wraps content in an HTML `<table>` element.
)md"sv,
};
constexpr URL_Behavior tel {
    u8"tel:",
    u8R"md(### Directive `tel`
```
tel(content: block): block
```
Creates a `tel:` hyperlink where the content is the phone number.

#### Example
`tel{+1-800-555-1234}` → `<a href="tel:+1-800-555-1234">+1-800-555-1234</a>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior tbody {
    HTML_Tag_Name(u8"tbody"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `tbody`
```
tbody(attr: pack named str, content: block): block
```
Wraps content in an HTML `<tbody>` table body element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior td {
    HTML_Tag_Name(u8"td"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `td`
```
td(attr: pack named str, content: block): block
```
Wraps content in an HTML `<td>` table data cell element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior tfoot {
    HTML_Tag_Name(u8"tfoot"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `tfoot`
```
tfoot(attr: pack named str, content: block): block
```
Wraps content in an HTML `<tfoot>` table footer element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior th {
    HTML_Tag_Name(u8"th"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `th`
```
th(attr: pack named str, content: block): block
```
Wraps content in an HTML `<th>` table header cell element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior thead {
    HTML_Tag_Name(u8"thead"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `thead`
```
thead(attr: pack named str, content: block): block
```
Wraps content in an HTML `<thead>` table header element.
)md"sv,
};
constexpr There_Behavior there {
    u8R"md(### Directive `there`
```
there(section: str, content: block): block
```
Stashes `content` into the named section.
The content is later inserted by `here` or `hereblock` with the same section name.

#### Example
`there(section: toc){...}` stashes content for insertion at `here(section: toc)`.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior tr {
    HTML_Tag_Name(u8"tr"),
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `tr`
```
tr(attr: pack named str, content: block): block
```
Wraps content in an HTML `<tr>` table row element.
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior tt {
    HTML_Tag_Name(u8"tt-"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `tt`
```
tt(attr: pack named str, content: block): block
```
Wraps content in a `<tt->` element for teletype/monospace styling.

#### Example
`tt{hello}` → `<tt->hello</tt->`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior u {
    HTML_Tag_Name(u8"u"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `u`
```
u(attr: pack named str, content: block): block
```
Wraps content in an HTML `<u>` underline element.

#### Example
`u{hello}` → `<u>hello</u>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior ul {
    html_tag::ul,
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `ul`
```
ul(attr: pack named str, content: block): block
```
Wraps content in an HTML `<ul>` unordered list element.
)md"sv,
};
constexpr URL_Behavior url {
    u8"",
    u8R"md(### Directive `url`
```
url(content: block): block
```
Creates a hyperlink where the content is the URL and serves as both href and label.

#### Example
`url{https://example.com}` → `<a href="https://example.com">https://example.com</a>`
)md"sv,
};
constexpr Fixed_Name_Passthrough_Behavior var {
    HTML_Tag_Name(u8"var"),
    Policy_Usage::inherit,
    Directive_Display::in_line,
    u8R"md(### Directive `var`
```
var(attr: pack named str, content: block): block
```
Wraps content in an HTML `<var>` variable element.

#### Example
`var{x}` → `<var>x</var>`
)md"sv,
};
constexpr Self_Closing_Behavior wbr {
    HTML_Tag_Name(u8"wbr"),
    Directive_Display::in_line,
    u8R"md(### Directive `wbr`
```
wbr(): block
```
Inserts an HTML `<wbr>` word break opportunity.
)md"sv,
};
constexpr In_Tag_Behavior wg21_grammar {
    HTML_Tag_Name(u8"dl"),
    u8"grammar",
    Policy_Usage::html,
    Directive_Display::block,
    u8R"md(### Directive `wg21_grammar`
```
wg21_grammar(attr: pack named str, content: block): block
```
Wraps content in a `<dl class="grammar">` element for WG21 grammar notation.
)md"sv,
};
constexpr WG21_Head_Behavior wg21_head {
    u8R"md(### Directive `wg21_head`
```
wg21_head(title: block, content: block): block
```
Generates a WG21-style document header `<div class="wg21-head">`.
`title` is the document title; `content` is the body of the header.
)md"sv,
};

// clang-format off
#define COWEL_DEPRECATED_ALIAS(name, use_instead)                                                  \
    constexpr Deprecated_Behavior name { use_instead, u8## #use_instead }
// clang-format on

// Usage:
// COWEL_DEPRECATED_ALIAS(abstract, Babstract);

struct Name_And_Behavior {
    std::u8string_view name;
    const Directive_Behavior* behavior;
};

#define COWEL_NAME_AND_BEHAVIOR_ENTRY(...) { u8## #__VA_ARGS__##sv, &__VA_ARGS__ }

constexpr Name_And_Behavior behaviors_by_name[] {
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Babstract),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdecision),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdel),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdetails),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bdiff),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bex),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bimp),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bindent),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bins),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bnote),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bquote),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Btip),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Btodo),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bug),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(Bwarn),
    { u8"__arg_source_as_text"sv, &internal_arg_source_as_text },
    { u8"__eq"sv, &internal_eq },
    { u8"__expect_error"sv, &internal_expect_error },
    { u8"__expect_fatal"sv, &internal_expect_fatal },
    { u8"__expect_warning"sv, &internal_expect_warning },
    { u8"__typeof"sv, &internal_typeof },
    COWEL_NAME_AND_BEHAVIOR_ENTRY(b),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(bib),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(blockquote),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(br),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(caption),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cite),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(code),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(codeblock),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(col),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(colgroup),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(comment),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_abs),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_actions),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_add),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_alias),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_and),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_as_text),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_bit_not),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_ceil),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_by_entity),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_by_name),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_by_num),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_get_name),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_char_get_num),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_div),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_div_to_neg_inf),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_div_to_pos_inf),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_div_to_zero),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_eq),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_floor),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_ge),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_gt),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_highlight),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_highlight_as),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_highlight_phantom),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_html_element),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_html_self_closing_element),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_include),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_include_text),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_invoke),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_le),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_lt),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_macro),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_max),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_min),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_mul),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_ne),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_nearest),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_nearest_away_zero),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_neg),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_no_invoke),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_not),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_or),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraph_enter),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraph_inherit),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraph_leave),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_paragraphs),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_pos),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_put),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_regex),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_reinterpret_as_float),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_reinterpret_as_int),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_rem_to_neg_inf),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_rem_to_pos_inf),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_rem_to_zero),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_source_as_text),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_sqrt),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_contains),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_find),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_length),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_match),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_replace_all),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_replace_first),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_substr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_to_lower),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_to_upper),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_utf8_length),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_sub),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_text_as_html),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_text_only),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_to_html),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_to_str),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_trunc),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_var_delete),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_var_exists),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_var_get),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_var_let),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_var_set),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dd),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(del),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(details),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dfn),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(div),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dl),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(dt),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(em),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(error),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(gterm),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h1),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h2),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h3),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h4),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h5),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(h6),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(here),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(hereblock),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(hr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(i),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ins),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(kbd),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(li),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(lorem_ipsum),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(mail),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(make_bib),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(make_contents),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(mark),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(math),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(mathblock),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(nobr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(noscript),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(o),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ol),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(p),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(pre),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(q),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ref),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(s),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(samp),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(sans),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(script),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(serif),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(small),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(span),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(strong),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(style),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(sub),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(summary),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(sup),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(table),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tbody),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(td),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tel),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tfoot),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(th),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(thead),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(there),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(tt),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(u),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(ul),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(url),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(var),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(wbr),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(wg21_grammar),
    COWEL_NAME_AND_BEHAVIOR_ENTRY(wg21_head),
};

static_assert(std::ranges::is_sorted(behaviors_by_name, {}, &Name_And_Behavior::name));

} // namespace

struct Builtin_Directive_Set::Impl {
    constexpr Impl() = default;
    constexpr ~Impl() = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

Builtin_Directive_Set::Builtin_Directive_Set()
    : m_impl(std::make_unique<Builtin_Directive_Set::Impl>())
{
}

Builtin_Directive_Set::~Builtin_Directive_Set() = default;

const Directive_Behavior& Builtin_Directive_Set::get_error_behavior() const noexcept
{
    return error;
}

Distant<std::u8string_view>
Builtin_Directive_Set::fuzzy_lookup_name(std::u8string_view name, Context& context) const
{
    static constexpr auto all_names = [] {
        std::array<std::u8string_view, std::size(behaviors_by_name)> result;
        std::ranges::transform(behaviors_by_name, result.data(), &Name_And_Behavior::name);
        return result;
    }();
    const Distant<std::size_t> result
        = closest_match(all_names, name, context.get_transient_memory());
    if (!result) {
        return {};
    }
    return { .value = all_names[result.value], .distance = result.distance };
}

const Directive_Behavior* Builtin_Directive_Set::operator()(std::u8string_view name) const
{
    const auto* const it
        = std::ranges::lower_bound(behaviors_by_name, name, {}, &Name_And_Behavior::name);
    if (it == std::end(behaviors_by_name) || it->name != name) {
        return nullptr;
    }
    return it->behavior;
}

} // namespace cowel

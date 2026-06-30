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

// clang-format off
constexpr Internal_Arg_Source_As_Text_Behavior internal_arg_source_as_text
  {};
constexpr Internal_Eq_Behavior internal_eq
  {};
constexpr Internal_Expect_Diagnostic_Behavior internal_expect_error
  { Processing_Status::error, Severity::error };
constexpr Internal_Expect_Diagnostic_Behavior internal_expect_fatal
  { Processing_Status::fatal, Severity::fatal };
constexpr Internal_Expect_Diagnostic_Behavior internal_expect_warning
  { Processing_Status::ok, Severity::warning };
constexpr Internal_Typeof_Behavior internal_typeof
  {};

constexpr Unary_Numeric_Expression_Behavior cowel_abs {
  Unary_Numeric_Expression_Kind::abs,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_abs"sv,
    .declaration = u8R"md(cowel_abs(x: int | float): int | float)md"sv,
    .description = u8R"md(Returns the absolute value of x.)md"sv,
    .example = u8R"md(`cowel_abs(-7)` → `7`.)md"sv,
  },
};
constexpr Policy_Behavior cowel_actions {
  Known_Content_Policy::actions,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_actions"sv,
    .declaration = u8R"md(cowel_actions(content: block): block)md"sv,
    .description = u8R"md(Processes content in actions policy; text output is ignored.)md"sv,
    .example = u8R"md(`cowel_actions{\cowel_macro("m"){...}}` defines a macro without text output.)md"sv,
  },
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_add {
  N_Ary_Numeric_Expression_Kind::add,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_add"sv,
    .declaration = u8R"md(cowel_add(args: pack int): int
cowel_add(args: pack float): float)md"sv,
    .description = u8R"md(Returns the sum of all arguments.)md"sv,
    .example = u8R"md(`cowel_add(1, 2, 3)` → `6`.)md"sv,
  },
};
constexpr Alias_Behavior cowel_alias {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_alias"sv,
    .declaration = u8R"md(cowel_alias(names: pack str, content: block): unit)md"sv,
    .description = u8R"md(Defines aliases for an existing directive.)md"sv,
    .example = u8R"md(`cowel_alias("N"){cowel_char_by_name}` makes `\N(...)` available.)md"sv,
  },
};
constexpr Logical_Expression_Behavior cowel_and {
  Logical_Expression_Kind::logical_and,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_and"sv,
    .declaration = u8R"md(cowel_and(args: pack lazy bool): bool)md"sv,
    .description = u8R"md(Logical AND with left-to-right short-circuiting.)md"sv,
    .example = u8R"md(`cowel_and(true, false)` → `false`.)md"sv,
  },
};
constexpr Policy_Behavior cowel_as_text {
  Known_Content_Policy::as_text,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_as_text"sv,
    .declaration = u8R"md(cowel_as_text(content: block): block)md"sv,
    .description = u8R"md(Reinterprets generated HTML as plain text.)md"sv,
    .example = u8R"md(`cowel_as_text{cowel_to_html{x < y}}` renders escaped text.)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_bit_not {
  Unary_Numeric_Expression_Kind::bit_not,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_bit_not"sv,
    .declaration = u8R"md(cowel_bit_not(x: int): int)md"sv,
    .description = u8R"md(Returns bitwise NOT of x.)md"sv,
    .example = u8R"md(`cowel_bit_not(0)` → `-1`.)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_ceil {
  Unary_Numeric_Expression_Kind::ceil,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_ceil"sv,
    .declaration = u8R"md(cowel_ceil(x: float): float)md"sv,
    .description = u8R"md(Rounds toward positive infinity.)md"sv,
    .example = u8R"md(`cowel_ceil(1.2)` → `2.0`.)md"sv,
  },
};
constexpr Char_By_Entity_Behavior cowel_char_by_entity {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_char_by_entity"sv,
    .declaration = u8R"md(cowel_char_by_entity(name: str): str)md"sv,
    .description = u8R"md(Returns a character from an HTML entity name (without `&` and `;`).)md"sv,
    .example = u8R"md(`cowel_char_by_entity("amp")` → `"&"`.)md"sv,
  },
};
constexpr Char_By_Name_Behavior cowel_char_by_name {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_char_by_name"sv,
    .declaration = u8R"md(cowel_char_by_name(name: str): str)md"sv,
    .description = u8R"md(Returns a Unicode code point by official name or alias.)md"sv,
    .example = u8R"md(`cowel_char_by_name("DIGIT ZERO")` → `"0"`.)md"sv,
  },
};
constexpr Char_By_Num_Behavior cowel_char_by_num {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_char_by_num"sv,
    .declaration = u8R"md(cowel_char_by_num(num: int): str)md"sv,
    .description = u8R"md(Returns the Unicode scalar value with numeric code point num.)md"sv,
    .example = u8R"md(`cowel_char_by_num(0x30)` → `"0"`.)md"sv,
  },
};
constexpr Char_Get_Name_Behavior cowel_char_get_name {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_char_get_name"sv,
    .declaration = u8R"md(cowel_char_get_name(x: str): str | null)md"sv,
    .description = u8R"md(Returns the Unicode code point name of the first code point in `x`.)md"sv,
    .example = u8R"md(`cowel_char_get_name("A")` → `LATIN CAPITAL LETTER A`.)md"sv,
  },
};
constexpr Char_Get_Num_Behavior cowel_char_get_num {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_char_get_num"sv,
    .declaration = u8R"md(cowel_char_get_num(x: str): int)md"sv,
    .description = u8R"md(Returns numeric value of the first code point in `x`.)md"sv,
    .example = u8R"md(`cowel_char_get_num("0")` → `48`.)md"sv,
  },
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_div {
  N_Ary_Numeric_Expression_Kind::div,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_div"sv,
    .declaration = u8R"md(cowel_div(args: pack float): float)md"sv,
    .description = u8R"md(Divides arguments left to right.)md"sv,
    .example = u8R"md(`cowel_div(20.0, 5.0, 2.0)` → `2.0`.)md"sv,
  },
};
constexpr Integer_Division_Expression_Behavior cowel_div_to_neg_inf {
  Integer_Division_Kind::div_to_neg_inf,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_div_to_neg_inf"sv,
    .declaration = u8R"md(cowel_div_to_neg_inf(x: int, y: int): int)md"sv,
    .description = u8R"md(Integer division rounded toward negative infinity.)md"sv,
    .example = u8R"md(`cowel_div_to_neg_inf(-3, 2)` → `-2`.)md"sv,
  },
};
constexpr Integer_Division_Expression_Behavior cowel_div_to_pos_inf {
  Integer_Division_Kind::div_to_pos_inf,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_div_to_pos_inf"sv,
    .declaration = u8R"md(cowel_div_to_pos_inf(x: int, y: int): int)md"sv,
    .description = u8R"md(Integer division rounded toward positive infinity.)md"sv,
    .example = u8R"md(`cowel_div_to_pos_inf(-3, 2)` → `-1`.)md"sv,
  },
};
constexpr Integer_Division_Expression_Behavior cowel_div_to_zero {
  Integer_Division_Kind::div_to_zero,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_div_to_zero"sv,
    .declaration = u8R"md(cowel_div_to_zero(x: int, y: int): int)md"sv,
    .description = u8R"md(Integer division rounded toward zero.)md"sv,
    .example = u8R"md(`cowel_div_to_zero(-3, 2)` → `-1`.)md"sv,
  },
};
constexpr Comparison_Expression_Behavior cowel_eq {
  Comparison_Expression_Kind::eq,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_eq"sv,
    .declaration = u8R"md(cowel_eq(x: unit, y: unit): bool
cowel_eq(x: null, y: null): bool
cowel_eq(x: bool, y: bool): bool
cowel_eq(x: int, y: int
cowel_eq(x: float, y: float): bool
cowel_eq(x: str, y: str): bool)md"sv,
    .description = u8R"md(Returns whether two values are equal.)md"sv,
    .example = u8R"md(`cowel_eq("a", "a")` → `true`.)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_floor {
  Unary_Numeric_Expression_Kind::floor,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_floor"sv,
    .declaration = u8R"md(cowel_floor(x: float): float)md"sv,
    .description = u8R"md(Rounds toward negative infinity.)md"sv,
    .example = u8R"md(`cowel_floor(1.9)` → `1.0`.)md"sv,
  },
};
constexpr Comparison_Expression_Behavior cowel_ge {
  Comparison_Expression_Kind::ge,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_ge"sv,
    .declaration = u8R"md(cowel_ge(x: int, y: int): int
cowel_ge(x: float, y: float): int
cowel_ge(x: str, y: str): int)md"sv,
    .description = u8R"md(Returns whether `x` ≥ `y`.)md"sv,
    .example = u8R"md(`cowel_ge(3, 2)` → `true`.)md"sv,
  },
};
constexpr Comparison_Expression_Behavior cowel_gt {
  Comparison_Expression_Kind::gt,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_gt"sv,
    .declaration = u8R"md(cowel_gt(x: int, y: int): int
cowel_gt(x: float, y: float): int
cowel_gt(x: str, y: str): int)md"sv,
    .description = u8R"md(Returns whether `x` > `y`.)md"sv,
    .example = u8R"md(`cowel_gt(3, 3)` → `false`.)md"sv,
  },
};
constexpr Highlight_Behavior cowel_highlight {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_highlight"sv,
    .declaration = u8R"md(cowel_highlight(lang: str, opaque: bool = false, content: block): block)md"sv,
    .description = u8R"md(Syntax-highlights content for the given language.)md"sv,
    .example = u8R"md(`cowel_highlight("cpp"){int x;}` highlights C++ tokens.)md"sv,
  },
};
constexpr Highlight_As_Behavior cowel_highlight_as {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_highlight_as"sv,
    .declaration = u8R"md(cowel_highlight_as(name: str, opaque: bool = false, content: block): block)md"sv,
    .description = u8R"md(Applies manual highlight category to content.)md"sv,
    .example = u8R"md(`cowel_highlight_as("keyword-type"){_Int128}` marks a type token.)md"sv,
  },
};
constexpr Policy_Behavior cowel_highlight_phantom {
  Known_Content_Policy::phantom,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_highlight_phantom"sv,
    .declaration = u8R"md(cowel_highlight_phantom(content: block): block)md"sv,
    .description = u8R"md(Feeds phantom text into highlighting without outputting it.)md"sv,
    .example = u8R"md(`cowel_highlight_phantom{"{"}` can affect nearby JSON tokenization.)md"sv,
  },
};
constexpr HTML_Element_Behavior cowel_html_element {
  HTML_Element_Self_Closing::normal,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_html_element"sv,
    .declaration = u8R"md(cowel_html_element(name: str, attr: group named str = (), content: block): block)md"sv,
    .description = u8R"md(Emits an HTML opening and closing tag with content.)md"sv,
    .example = u8R"md(`cowel_html_element("span", (id="x")){hello}` → `<span id="x">hello</span>`.)md"sv,
  },
};
constexpr HTML_Element_Behavior cowel_html_self_closing_element {
  HTML_Element_Self_Closing::self_closing,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_html_self_closing_element"sv,
    .declaration = u8R"md(cowel_html_self_closing_element(name: str, attr: group named str = (), content: block): block)md"sv,
    .description = u8R"md(Emits a self-closing HTML element. Content is ignored.)md"sv,
    .example = u8R"md(`cowel_html_self_closing_element("hr")` → `<hr />`.)md"sv,
  },
};
constexpr Include_Behavior cowel_include {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_include"sv,
    .declaration = u8R"md(cowel_include(path: str): block)md"sv,
    .description = u8R"md(Loads and processes another COWEL file in current policy.)md"sv,
    .example = u8R"md(`cowel_include("parts/header.cow")` includes that sub-document.)md"sv,
  },
};
constexpr Include_Text_Behavior cowel_include_text {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_include_text"sv,
    .declaration = u8R"md(cowel_include_text(path: str): str)md"sv,
    .description = u8R"md(Returns UTF-8 text from a file.)md"sv,
    .example = u8R"md(`cowel_include_text("example.js")` returns file contents as text.)md"sv,
  },
};
constexpr Invoke_Behavior cowel_invoke {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_invoke"sv,
    .declaration = u8R"md(cowel_invoke(name: str, content: block): any)md"sv,
    .description = u8R"md(Dynamically invokes a directive by name.)md"sv,
    .example = u8R"md(`cowel_invoke("cowel_char_by_name"){DIGIT ZERO}` calls it dynamically.)md"sv,
  },
};
constexpr Comparison_Expression_Behavior cowel_le {
  Comparison_Expression_Kind::le,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_le"sv,
    .declaration = u8R"md(cowel_le(x: int, y: int): int
cowel_le(x: float, y: float): int
cowel_le(x: str, y: str): int)md"sv,
    .description = u8R"md(Returns whether `x` ≤ `y`.)md"sv,
    .example = u8R"md(`cowel_le(2, 2)` → `true`.)md"sv,
  },
};
constexpr Comparison_Expression_Behavior cowel_lt {
  Comparison_Expression_Kind::lt,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_lt"sv,
    .declaration = u8R"md(cowel_lt(x: int, y: int): int
cowel_lt(x: float, y: float): int
cowel_lt(x: str, y: str): int)md"sv,
    .description = u8R"md(Returns whether `x` < `y`.)md"sv,
    .example = u8R"md(`cowel_lt("a", "b")` → `true`.)md"sv,
  },
};
constexpr Macro_Behavior cowel_macro {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_macro"sv,
    .declaration = u8R"md(cowel_macro(names: pack str, content: block): unit)md"sv,
    .description = u8R"md(Defines one or more macros.)md"sv,
    .example = u8R"md(```cowel
\: Define macro named "m"
\cowel_macro("m"){Hello}
\: Generates "Hello"
\m
```)md"sv,
  },
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_max {
  N_Ary_Numeric_Expression_Kind::max,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_max"sv,
    .declaration = u8R"md(cowel_max(args: pack int): int
cowel_max(args: pack float): float)md"sv,
    .description = u8R"md(Returns the greatest argument.)md"sv,
    .example = u8R"md(`cowel_max(3, 9, 4)` → `9`.)md"sv,
  },
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_min {
  N_Ary_Numeric_Expression_Kind::min,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_min"sv,
    .declaration = u8R"md(cowel_min(args: pack int): int
cowel_min(args: pack float): float)md"sv,
    .description = u8R"md(Returns the lowest argument.)md"sv,
    .example = u8R"md(`cowel_min(3, 9, 4)` → `3`.)md"sv,
  },
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_mul {
  N_Ary_Numeric_Expression_Kind::mul,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_mul"sv,
    .declaration = u8R"md(cowel_mul(args: pack int): int
cowel_mul(args: pack float): int)md"sv,
    .description = u8R"md(Returns product of all arguments.)md"sv,
    .example = u8R"md(`cowel_mul(2, 3, 4)` → `24`.)md"sv,
  },
};
constexpr Comparison_Expression_Behavior cowel_ne {
  Comparison_Expression_Kind::ne,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_ne"sv,
    .declaration = u8R"md(cowel_ne(x: unit, y: unit): bool
cowel_ne(x: null, y: null): bool
cowel_ne(x: bool, y: bool): bool
cowel_ne(x: int, y: int
cowel_ne(x: float, y: float): bool
cowel_ne(x: str, y: str): bool)md"sv,
    .description = u8R"md(Returns whether two values are not equal.)md"sv,
    .example = u8R"md(`cowel_ne("a", "b")` → `true`.)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_nearest {
  Unary_Numeric_Expression_Kind::nearest,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_nearest"sv,
    .declaration = u8R"md(cowel_nearest(x: float): float)md"sv,
    .description = u8R"md(Rounds to the nearest integer, ties to even.)md"sv,
    .example = u8R"md(```cowel
\cowel_nearest(2.4) \: 2.0
\cowel_nearest(2.5) \: 2.0 (exact tie)
\cowel_nearest(2.6) \: 3.0
```)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_nearest_away_zero {
  Unary_Numeric_Expression_Kind::nearest_away_zero,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_nearest_away_zero"sv,
    .declaration = u8R"md(cowel_nearest_away_zero(x: float): float)md"sv,
    .description = u8R"md(Rounds to the nearest integer, ties away from zero.)md"sv,
    .example = u8R"md(```cowel
\cowel_nearest_away_zero(2.4) \: 2.0
\cowel_nearest_away_zero(2.5) \: 3.0 (exact tie)
\cowel_nearest_away_zero(2.6) \: 3.0
```)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_neg {
  Unary_Numeric_Expression_Kind::neg,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_neg"sv,
    .declaration = u8R"md(cowel_neg(x: int): int
cowel_neg(x: float): float)md"sv,
    .description = u8R"md(Returns arithmetic negation of `x`.)md"sv,
    .example = u8R"md(`cowel_neg(7)` → `-7`.)md"sv,
  },
};
constexpr Policy_Behavior cowel_no_invoke {
  Known_Content_Policy::no_invoke,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_no_invoke"sv,
    .declaration = u8R"md(cowel_no_invoke(content: block): block)md"sv,
    .description = u8R"md(Treats directive invocations as source text.)md"sv,
    .example = u8R"md(`cowel_no_invoke{\awoo}` outputs literal `\awoo`.)md"sv,
  },
};
constexpr Logical_Not_Behavior cowel_not {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_not"sv,
    .declaration = u8R"md(cowel_not(x: bool): bool)md"sv,
    .description = u8R"md(Logical NOT.)md"sv,
    .example = u8R"md(`cowel_not(true)` → `false`.)md"sv,
  },
};
constexpr Logical_Expression_Behavior cowel_or {
  Logical_Expression_Kind::logical_or,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_or"sv,
    .declaration = u8R"md(cowel_or(args: pack lazy bool): bool)md"sv,
    .description = u8R"md(Logical OR with left-to-right short-circuiting.)md"sv,
    .example = u8R"md(`cowel_or(false, true)` → `true`.)md"sv,
  },
};
constexpr Policy_Behavior cowel_paragraphs {
  Known_Content_Policy::paragraphs,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_paragraphs"sv,
    .declaration = u8R"md(cowel_paragraphs(content: block): block)md"sv,
    .description = u8R"md(Processes content with paragraph splitting.)md"sv,
    .example = u8R"md(`cowel_paragraphs{A\n\nB}` emits two paragraphs.)md"sv,
  },
};
constexpr Paragraph_Enter_Behavior cowel_paragraph_enter {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_paragraph_enter"sv,
    .declaration = u8R"md(cowel_paragraph_enter(): block)md"sv,
    .description = u8R"md(In paragraphs policy, opens a paragraph if currently outside one.)md"sv,
    .example = u8R"md(Use before text to force paragraph start.)md"sv,
  },
};
constexpr Paragraph_Inherit_Behavior cowel_paragraph_inherit {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_paragraph_inherit"sv,
    .declaration = u8R"md(cowel_paragraph_inherit(): block)md"sv,
    .description = u8R"md(Marks generated content as inheriting surrounding paragraph splitting.)md"sv,
    .example = u8R"md(Used in programmatic directives to emulate macro/include inheritance.)md"sv,
  },
};
constexpr Paragraph_Leave_Behavior cowel_paragraph_leave {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_paragraph_leave"sv,
    .declaration = u8R"md(cowel_paragraph_leave(): block)md"sv,
    .description = u8R"md(In paragraphs policy, closes current paragraph if one is open.)md"sv,
    .example = u8R"md(Often used before block-only HTML like `hr`.)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_pos {
  Unary_Numeric_Expression_Kind::pos,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_pos"sv,
    .declaration = u8R"md(cowel_pos(x: int): int
cowel_pos(x: float): float)md"sv,
    .description = u8R"md(Returns `x` unchanged.)md"sv,
    .example = u8R"md(`cowel_pos(-4)` → `-4`.)md"sv,
  },
};
constexpr Put_Behavior cowel_put {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_put"sv,
    .declaration = u8R"md(cowel_put(else?: lazy any, content?: block): any)md"sv,
    .description = u8R"md(In macro expansion, inserts supplied content or arguments.)md"sv,
    .example = u8R"md(If `m` is `cowel_macro("m"){\cowel_put{0}}`, then `\m("x")` emits `x`.)md"sv,
  },
};
constexpr Regex_Make_Behavior cowel_regex {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_regex"sv,
    .declaration = u8R"md(cowel_regex(
  pattern: str,
  flags: str = "",
): regex)md"sv,
    .description = u8R"md(Compiles a regular expression value.)md"sv,
    .example = u8R"md(`cowel_str_match("awoo", cowel_regex("awo+"))` → `true`.)md"sv,
  },
};
constexpr Reinterpret_As_Float_Behavior cowel_reinterpret_as_float {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_reinterpret_as_float"sv,
    .declaration = u8R"md(cowel_reinterpret_as_float(x: int): float)md"sv,
    .description = u8R"md(Reinterprets integer bits as binary64 float.)md"sv,
    .example = u8R"md(`cowel_reinterpret_as_float(0x3ff8000000000000)` → `1.5`.)md"sv,
  },
};
constexpr Reinterpret_As_Int_Behavior cowel_reinterpret_as_int {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_reinterpret_as_int"sv,
    .declaration = u8R"md(cowel_reinterpret_as_int(x: float): int)md"sv,
    .description = u8R"md(Reinterprets float bits as integer.)md"sv,
    .example = u8R"md(`cowel_reinterpret_as_int(1.5)` → `0x3ff8000000000000`.)md"sv,
  },
};
constexpr Integer_Division_Expression_Behavior cowel_rem_to_neg_inf {
  Integer_Division_Kind::rem_to_neg_inf,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_rem_to_neg_inf"sv,
    .declaration = u8R"md(cowel_rem_to_neg_inf(x: int, y: int): int)md"sv,
    .description = u8R"md(Remainder of division `x / y` with rounding towards negative infinity..)md"sv,
    .example = u8R"md(`cowel_rem_to_neg_inf(-3, 2)` → `1`.)md"sv,
  },
};
constexpr Integer_Division_Expression_Behavior cowel_rem_to_pos_inf {
  Integer_Division_Kind::rem_to_pos_inf,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_rem_to_pos_inf"sv,
    .declaration = u8R"md(cowel_rem_to_pos_inf(x: int, y: int): int)md"sv,
    .description = u8R"md(Remainder of division `x / y` with rounding towards positive infinity.)md"sv,
    .example = u8R"md(`cowel_rem_to_pos_inf(-3, 2)` → `-1`.)md"sv,
  },
};
constexpr Integer_Division_Expression_Behavior cowel_rem_to_zero {
  Integer_Division_Kind::rem_to_zero,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_rem_to_zero"sv,
    .declaration = u8R"md(cowel_rem_to_zero(x: int, y: int): int)md"sv,
    .description = u8R"md(Remainder of division `x / y` with rounding towards zero.)md"sv,
    .example = u8R"md(`cowel_rem_to_zero(-3, 2)` → `-1`.)md"sv,
  },
};
constexpr Policy_Behavior cowel_source_as_text {
  Known_Content_Policy::source_as_text,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_source_as_text"sv,
    .declaration = u8R"md(cowel_source_as_text(content: block): block)md"sv,
    .description = u8R"md(Treats content as literal COWEL source text.)md"sv,
    .example = u8R"md(`cowel_source_as_text{\d}` emits `\d` literally.)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_sqrt {
  Unary_Numeric_Expression_Kind::sqrt,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_sqrt"sv,
    .declaration = u8R"md(cowel_sqrt(x: float): float)md"sv,
    .description = u8R"md(Returns square root of x.)md"sv,
    .example = u8R"md(`cowel_sqrt(9.0)` → `3.0`.)md"sv,
  },
};
constexpr Str_At_Behavior cowel_str_at {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_at"sv,
    .declaration = u8R"md(cowel_str_at(text: str, index: int): str)md"sv,
    .description = u8R"md(Returns the code point at the given index.)md"sv,
    .example = u8R"md(`cowel_str_at("awoo", 1)` → `w`.)md"sv,
  },
};
constexpr Str_Contains_Behavior cowel_str_contains {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_contains"sv,
    .declaration = u8R"md(cowel_str_contains(text: str, needle: str | regex): bool)md"sv,
    .description = u8R"md(Returns whether needle occurs in text.)md"sv,
    .example = u8R"md(`cowel_str_contains("awoo", "o")` → `true`.)md"sv,
  },
};
constexpr Str_Find_Behavior cowel_str_find {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_find"sv,
    .declaration = u8R"md(cowel_str_find(text: str, needle: str | regex): int | null)md"sv,
    .description = u8R"md(Returns first match index in code points, or `null`.)md"sv,
    .example = u8R"md(`cowel_str_find("awoo", "o")` → `2`.)md"sv,
  },
};
constexpr Str_Substr_Behavior cowel_str_substr {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_substr"sv,
    .declaration = u8R"md(cowel_str_substr(
  text: str,
  start: int,
  length?: int,
): str)md"sv,
    .description = u8R"md(Extracts substring by code-point index.)md"sv,
    .example = u8R"md(`cowel_str_substr("awoo", 1, 2)` → `wo`.)md"sv,
  },
};
constexpr Str_Length_Behavior cowel_str_length {
  Str_Length_Kind::code_point,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_length"sv,
    .declaration = u8R"md(cowel_str_length(x: str): int)md"sv,
    .description = u8R"md(Returns length in Unicode code points.)md"sv,
    .example = u8R"md(`cowel_str_length("a")` → `1`.)md"sv,
  },
};
constexpr Str_Match_Behavior cowel_str_match {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_match"sv,
    .declaration = u8R"md(cowel_str_match(text: str, regex: regex): bool)md"sv,
    .description = u8R"md(Returns whether the whole string matches regex.)md"sv,
    .example = u8R"md(`cowel_str_match("awoo", cowel_regex("awo+"))` → `true`.)md"sv,
  },
};
constexpr Str_Replace_Behavior cowel_str_replace_all {
  Str_Replacement_Kind::all,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_replace_all"sv,
    .declaration = u8R"md(cowel_str_replace_all(
  text: str,
  needle: str | regex,
  with: str,
): str)md"sv,
    .description = u8R"md(Replaces all matches of `needle` in `text` with `with`.)md"sv,
    .example = u8R"md(`cowel_str_replace_all("awoo", "o", "x")` → `awxx`.)md"sv,
  },
};
constexpr Str_Replace_Behavior cowel_str_replace_first {
  Str_Replacement_Kind::first,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_replace_first"sv,
    .declaration = u8R"md(cowel_str_replace_first(
  text: str,
  needle: str | regex,
  with: str,
): str)md"sv,
    .description = u8R"md(Replaces the first match of `needle` in `text` with `with`.)md"sv,
    .example = u8R"md(`cowel_str_replace_first("awoo", "o", "x")` → `awxo`.)md"sv,
  },
};
constexpr Str_Transform_Behavior cowel_str_to_lower {
  Text_Transformation::lowercase,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_to_lower"sv,
    .declaration = u8R"md(cowel_str_to_lower(x: str): str)md"sv,
    .description = u8R"md(Converts to lowercase with Unicode default mapping.)md"sv,
    .example = u8R"md(`cowel_str_to_lower("Awoo")` → `awoo`.)md"sv,
  },
};
constexpr Str_Transform_Behavior cowel_str_to_upper {
  Text_Transformation::uppercase,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_to_upper"sv,
    .declaration = u8R"md(cowel_str_to_upper(x: str): str)md"sv,
    .description = u8R"md(Converts to uppercase with Unicode default mapping.)md"sv,
    .example = u8R"md(`cowel_str_to_upper("Awoo")` → `AWOO`.)md"sv,
  },
};
constexpr Str_Length_Behavior cowel_str_utf8_length {
  Str_Length_Kind::utf8,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_str_utf8_length"sv,
    .declaration = u8R"md(cowel_str_utf8_length(x: str): int)md"sv,
    .description = u8R"md(Returns length in UTF-8 code units.)md"sv,
    .example = u8R"md(`cowel_str_utf8_length("a")` → `1`.)md"sv,
  },
};
constexpr N_Ary_Numeric_Expression_Behavior cowel_sub {
  N_Ary_Numeric_Expression_Kind::sub,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_sub"sv,
    .declaration = u8R"md(cowel_sub(args: pack int): int
cowel_sub(args: pack float): float)md"sv,
    .description = u8R"md(Subtracts arguments left to right.)md"sv,
    .example = u8R"md(`cowel_sub(10, 3, 2)` → `5`.)md"sv,
  },
};
constexpr Policy_Behavior cowel_text_as_html {
  Known_Content_Policy::text_as_html,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_text_as_html"sv,
    .declaration = u8R"md(cowel_text_as_html(content: block): block)md"sv,
    .description = u8R"md(Treats text as HTML markup.)md"sv,
    .example = u8R"md(`cowel_text_as_html{<strong>x</strong>}` emits HTML tags.)md"sv,
  },
};
constexpr Policy_Behavior cowel_text_only {
  Known_Content_Policy::text_only,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_text_only"sv,
    .declaration = u8R"md(cowel_text_only(content: block): block)md"sv,
    .description = u8R"md(Keeps plaintext and strips HTML output.)md"sv,
    .example = u8R"md(`cowel_text_only{Hello, \cowel_html_element("strong"){x}}` yields `Hello, x`.)md"sv,
  },
};
constexpr Policy_Behavior cowel_to_html {
  Known_Content_Policy::to_html,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_to_html"sv,
    .declaration = u8R"md(cowel_to_html(content: block): block)md"sv,
    .description = u8R"md(Processes content with to-HTML policy.)md"sv,
    .example = u8R"md(`cowel_to_html{x < y}` emits HTML-safe output.)md"sv,
  },
};
constexpr To_Str_Behavior cowel_to_str {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_to_str"sv,
    .declaration = u8R"md(cowel_to_str(x: unit | bool | str | block): str
cowel_to_str(
  x: int,
  base: int = 10,
  zpad: int = 0,
): str
cowel_to_str(
  x: float,
  format: str = "splice",
): str)md"sv,
    .description = u8R"md(Converts a value to string, with numeric formatting options.)md"sv,
    .example = u8R"md(`cowel_to_str(255, base=16)` → `ff`.)md"sv,
  },
};
constexpr Unary_Numeric_Expression_Behavior cowel_trunc {
  Unary_Numeric_Expression_Kind::trunc,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_trunc"sv,
    .declaration = u8R"md(cowel_trunc(x: float): float)md"sv,
    .description = u8R"md(Rounds `x` toward zero.)md"sv,
    .example = u8R"md(`cowel_trunc(-1.9)` → `-1.0`.)md"sv,
  },
};
constexpr Var_Delete_Behavior cowel_var_delete {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_var_delete"sv,
    .declaration = u8R"md(cowel_var_delete(name: str): unit)md"sv,
    .description = u8R"md(Deletes a variable binding.)md"sv,
    .example = u8R"md(`cowel_var_delete("x")` removes x.)md"sv,
  },
};
constexpr Var_Exists_Behavior cowel_var_exists {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_var_exists"sv,
    .declaration = u8R"md(cowel_var_exists(name: str): bool)md"sv,
    .description = u8R"md(Returns whether a variable exists.)md"sv,
    .example = u8R"md(`cowel_var_exists("x")` → `true` if x is defined.)md"sv,
  },
};
constexpr Var_Get_Behavior cowel_var_get {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_var_get"sv,
    .declaration = u8R"md(cowel_var_get(name: str): any)md"sv,
    .description = u8R"md(Returns value of a variable.)md"sv,
    .example = u8R"md(After `cowel_var_let("x", 3)`, `cowel_var_get("x")` → `3`.)md"sv,
  },
};
constexpr Var_Let_Behavior cowel_var_let {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_var_let"sv,
    .declaration = u8R"md(cowel_var_let(name: str, value: any): unit)md"sv,
    .description = u8R"md(Defines a new variable.)md"sv,
    .example = u8R"md(`cowel_var_let("x", 3)` creates x.)md"sv,
  },
};
constexpr Var_Set_Behavior cowel_var_set {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cowel_var_set"sv,
    .declaration = u8R"md(cowel_var_set(name: str, value: any): unit)md"sv,
    .description = u8R"md(Updates an existing variable.)md"sv,
    .example = u8R"md(After `cowel_var_let("x", 1)`, `cowel_var_set("x", 2)` updates x.)md"sv,
  },
};

// Legacy directives
constexpr Fixed_Name_Passthrough_Behavior b {
  html_tag::b,
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"b"sv,
    .declaration = u8R"md(b(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Makes content bold.)md"sv,
    .example = u8R"md(`b{hello}` → `<b>hello</b>`)md"sv,
  },
};
constexpr Special_Block_Behavior Babstract {
  HTML_Tag_Name(u8"abstract-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Babstract"sv,
    .declaration = u8R"md(Babstract(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates an `<abstract-block>` callout with an intro label.)md"sv,
  },
};
constexpr Special_Block_Behavior Bdecision {
  HTML_Tag_Name(u8"decision-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bdecision"sv,
    .declaration = u8R"md(Bdecision(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<decision-block>` callout with an intro label.)md"sv,
  },
};
constexpr Special_Block_Behavior Bdel {
  HTML_Tag_Name(u8"del-block"),
  Intro_Policy::no,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bdel"sv,
    .declaration = u8R"md(Bdel(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<del-block>` block for marking deleted content.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior Bdetails {
  HTML_Tag_Name(u8"details"),
  Policy_Usage::inherit,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bdetails"sv,
    .declaration = u8R"md(Bdetails(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<details>` disclosure block element.)md"sv,
  },
};
constexpr Special_Block_Behavior Bdiff {
  HTML_Tag_Name(u8"diff-block"),
  Intro_Policy::no,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bdiff"sv,
    .declaration = u8R"md(Bdiff(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<diff-block>` block for showing diff content.)md"sv,
  },
};
constexpr Special_Block_Behavior Bex {
  HTML_Tag_Name(u8"example-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bex"sv,
    .declaration = u8R"md(Bex(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates an `<example-block>` callout with an intro label.)md"sv,
  },
};
constexpr Bibliography_Add_Behavior bib {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"bib"sv,
    .declaration = u8R"md(bib(
  id: str,
  title?: str,
  date?: str,
  publisher?: str,
  link?: str,
  long_link?: str,
  author?: str,
): block)md"sv,
    .description = u8R"md(Adds a bibliography entry.
The entry can be referenced with `\ref`.)md"sv,
    .example = u8R"md(`bib(id="ISO23", title="C++23 Standard", date="2024")` registers a bibliography entry.)md"sv,
  },
};
constexpr Special_Block_Behavior Bimp {
  HTML_Tag_Name(u8"important-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bimp"sv,
    .declaration = u8R"md(Bimp(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates an `<important-block>` callout with an intro label.)md"sv,
  },
};
constexpr In_Tag_Behavior Bindent {
  html_tag::div,
  u8"indent",
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bindent"sv,
    .declaration = u8R"md(Bindent(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in a `<div class="indent">` for indented block content.)md"sv,
  },
};
constexpr Special_Block_Behavior Bins {
  HTML_Tag_Name(u8"ins-block"),
  Intro_Policy::no,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bins"sv,
    .declaration = u8R"md(Bins(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates an `<ins-block>` block for marking inserted content.)md"sv,
  },
};
constexpr Special_Block_Behavior blockquote {
  HTML_Tag_Name(u8"blockquote"),
  Intro_Policy::no,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"blockquote"sv,
    .declaration = u8R"md(blockquote(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates an HTML `<blockquote>` element.)md"sv,
  },
};
constexpr Special_Block_Behavior Bnote {
  HTML_Tag_Name(u8"note-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bnote"sv,
    .declaration = u8R"md(Bnote(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<note-block>` callout with an intro label.)md"sv,
  },
};
constexpr Special_Block_Behavior Bquote {
  HTML_Tag_Name(u8"blockquote"),
  Intro_Policy::no,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bquote"sv,
    .declaration = u8R"md(Bquote(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates an HTML `<blockquote>` element.
Alias of `blockquote`.)md"sv,
  },
};
constexpr Self_Closing_Behavior br {
  html_tag::br,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"br"sv,
    .declaration = u8R"md(br(): block)md"sv,
    .description = u8R"md(Inserts an HTML `<br>` line break.)md"sv,
    .example = u8R"md(`br` or `br{}` → `<br>`)md"sv,
  },
};
constexpr Special_Block_Behavior Btip {
  HTML_Tag_Name(u8"tip-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Btip"sv,
    .declaration = u8R"md(Btip(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<tip-block>` callout with an intro label.)md"sv,
  },
};
constexpr Special_Block_Behavior Btodo {
  HTML_Tag_Name(u8"todo-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Btodo"sv,
    .declaration = u8R"md(Btodo(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<todo-block>` callout with an intro label.)md"sv,
  },
};
constexpr Special_Block_Behavior Bug {
  HTML_Tag_Name(u8"bug-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bug"sv,
    .declaration = u8R"md(Bug(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<bug-block>` callout with an intro label.)md"sv,
  },
};
constexpr Special_Block_Behavior Bwarn {
  HTML_Tag_Name(u8"warning-block"),
  Intro_Policy::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"Bwarn"sv,
    .declaration = u8R"md(Bwarn(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a `<warning-block>` callout with an intro label.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior caption {
  HTML_Tag_Name(u8"caption"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"caption"sv,
    .declaration = u8R"md(caption(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<caption>` element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior cite {
  HTML_Tag_Name(u8"cite"),
  Policy_Usage::html,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"cite"sv,
    .declaration = u8R"md(cite(
    attr: pack named str,
    content: block,
  ): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<cite>` citation element.)md"sv,
    .example = u8R"md(`cite{My Book}` → `<cite>My Book</cite>`)md"sv,
  },
};
constexpr Code_Behavior code {
  HTML_Tag_Name(u8"code"),
  Directive_Display::in_line,
  Pre_Trimming::no,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"code"sv,
    .declaration = u8R"md(code(
    lang: str,
    content: block,
  ): block)md"sv,
    .description = u8R"md(Renders content as syntax-highlighted inline code in a `<code>` element.)md"sv,
    .example = u8R"md(`code("cpp"){int}` → `<code><h- data-h=kw>int</h-></code>`)md"sv,
  },
};
constexpr Code_Behavior codeblock {
  HTML_Tag_Name(u8"code-block"),
  Directive_Display::block,
  Pre_Trimming::yes,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"codeblock"sv,
    .declaration = u8R"md(codeblock(
    lang: str,
    content: block,
  ): block)md"sv,
    .description = u8R"md(Renders content as a syntax-highlighted code block in a `<code-block>` element.)md"sv,
    .example = u8R"md(`codeblock("cpp"){int main() {}}` produces a highlighted block.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior col {
  HTML_Tag_Name(u8"col"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"col"sv,
    .declaration = u8R"md(col(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<col>` table column element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior colgroup {
  HTML_Tag_Name(u8"colgroup"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"colgroup"sv,
    .declaration = u8R"md(colgroup(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<colgroup>` table column group element.)md"sv,
  },
};
constexpr Comment_Behavior comment {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"comment"sv,
    .declaration = u8R"md(comment(content: block): unit)md"sv,
    .description = u8R"md(Discards its content — acts as a document comment.)md"sv,
    .example = u8R"md(`comment{This text will not appear in output.}` emits nothing.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior dd {
  HTML_Tag_Name(u8"dd"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"dd"sv,
    .declaration = u8R"md(dd(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<dd>` description definition element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior del {
  HTML_Tag_Name(u8"del"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"del"sv,
    .declaration = u8R"md(del(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<del>` element for deleted text.)md"sv,
    .example = u8R"md(`del{old text}` → `<del>old text</del>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior details {
  HTML_Tag_Name(u8"details"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"details"sv,
    .declaration = u8R"md(details(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<details>` disclosure element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior dfn {
  HTML_Tag_Name(u8"dfn"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"dfn"sv,
    .declaration = u8R"md(dfn(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<dfn>` definition term element.)md"sv,
    .example = u8R"md(`dfn{iterator}` → `<dfn>iterator</dfn>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior div {
  html_tag::div,
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"div"sv,
    .declaration = u8R"md(div(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<div>` block element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior dl {
  HTML_Tag_Name(u8"dl"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"dl"sv,
    .declaration = u8R"md(dl(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<dl>` description list element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior dt {
  HTML_Tag_Name(u8"dt"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"dt"sv,
    .declaration = u8R"md(dt(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<dt>` description term element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior em {
  HTML_Tag_Name(u8"em"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"em"sv,
    .declaration = u8R"md(em(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<em>` emphasis element.)md"sv,
    .example = u8R"md(`em{hello}` → `<em>hello</em>`)md"sv,
  },
};
constexpr Error_Behavior error {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"error"sv,
    .declaration = u8R"md(error(content: block): block)md"sv,
    .description = u8R"md(Renders content as a visible error in an `<error->` element.
Typically used to display error output inline.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior gterm {
  HTML_Tag_Name(u8"g-term"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"gterm"sv,
    .declaration = u8R"md(gterm(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in a `<g-term>` grammar term element.)md"sv,
  },
};
constexpr Heading_Behavior h1 {
  1,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"h1"sv,
    .declaration = u8R"md(h1(
  id: str,
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a level 1 `<h1>` heading.
Headings are automatically numbered and added to the table of contents.)md"sv,
    .example = u8R"md(`h1{Introduction}` → `<h1>Introduction</h1>`)md"sv,
  },
};
constexpr Heading_Behavior h2 {
  2,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"h2"sv,
    .declaration = u8R"md(h2(
  id: str,
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a level 2 `<h2>` heading.
Headings are automatically numbered and added to the table of contents.)md"sv,
  },
};
constexpr Heading_Behavior h3 {
  3,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"h3"sv,
    .declaration = u8R"md(h3(
  id: str,
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a level 3 `<h3>` heading.
Headings are automatically numbered and added to the table of contents.)md"sv,
  },
};
constexpr Heading_Behavior h4 {
  4,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"h4"sv,
    .declaration = u8R"md(h4(
  id: str,
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a level 4 `<h4>` heading.
Headings are automatically numbered and added to the table of contents.)md"sv,
  },
};
constexpr Heading_Behavior h5 {
  5,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"h5"sv,
    .declaration = u8R"md(h5(
  id: str,
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a level 5 `<h5>` heading.
Headings are automatically numbered and added to the table of contents.)md"sv,
  },
};
constexpr Heading_Behavior h6 {
  6,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"h6"sv,
    .declaration = u8R"md(h6(
  id: str,
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Creates a level 6 `<h6>` heading.
Headings are automatically numbered and added to the table of contents.)md"sv,
  },
};
constexpr Here_Behavior here {
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"here"sv,
    .declaration = u8R"md(here(
  section: str,
  content: block,
): block)md"sv,
    .description = u8R"md(Inserts previously stashed section content inline at this point.
Use `there` to stash content for a named section.)md"sv,
  },
};
constexpr Here_Behavior hereblock {
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"hereblock"sv,
    .declaration = u8R"md(hereblock(
  section: str,
  content: block,
): block)md"sv,
    .description = u8R"md(Inserts previously stashed section content as a block at this point.
Use `there` to stash content for a named section.)md"sv,
  },
};
constexpr Self_Closing_Behavior hr {
  HTML_Tag_Name(u8"hr"),
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"hr"sv,
    .declaration = u8R"md(hr(): block)md"sv,
    .description = u8R"md(Inserts an HTML `<hr>` horizontal rule.)md"sv,
    .example = u8R"md(`hr` or `hr{}` → `<hr>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior i {
  HTML_Tag_Name(u8"i"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"i"sv,
    .declaration = u8R"md(i(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<i>` italic element.)md"sv,
    .example = u8R"md(`i{hello}` → `<i>hello</i>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior ins {
  HTML_Tag_Name(u8"ins"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"ins"sv,
    .declaration = u8R"md(ins(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<ins>` element for inserted text.)md"sv,
    .example = u8R"md(`ins{new text}` → `<ins>new text</ins>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior kbd {
  HTML_Tag_Name(u8"kbd"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"kbd"sv,
    .declaration = u8R"md(kbd(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<kbd>` keyboard input element.)md"sv,
    .example = u8R"md(`kbd{Ctrl+C}` → `<kbd>Ctrl+C</kbd>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior li {
  HTML_Tag_Name(u8"li"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"li"sv,
    .declaration = u8R"md(li(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<li>` list item element.)md"sv,
  },
};
constexpr Lorem_Ipsum_Behavior lorem_ipsum {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"lorem_ipsum"sv,
    .declaration = u8R"md(lorem_ipsum(): block)md"sv,
    .description = u8R"md(Outputs a standard Lorem Ipsum placeholder paragraph.)md"sv,
    .example = u8R"md(`lorem_ipsum` emits a paragraph of Lorem Ipsum text.)md"sv,
  },
};
constexpr URL_Behavior mail {
  u8"mailto:",
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"mail"sv,
    .declaration = u8R"md(mail(content: block): block)md"sv,
    .description = u8R"md(Creates a `mailto:` hyperlink where the content is the email address.)md"sv,
    .example = u8R"md(`mail{user@example.com}` → `<a href="mailto:user@example.com">user@example.com</a>`)md"sv,
  },
};
constexpr Make_Section_Behavior make_bib {
  Directive_Display::block,
  class_name::bibliography,
  section_name::bibliography,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"make_bib"sv,
    .declaration = u8R"md(make_bib(): block)md"sv,
    .description = u8R"md(Inserts the bibliography section at this location.
Bibliography entries must be added with `bib` beforehand.)md"sv,
  },
};
constexpr Make_Section_Behavior make_contents {
  Directive_Display::block,
  class_name::table_of_contents,
  section_name::table_of_contents,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"make_contents"sv,
    .declaration = u8R"md(make_contents(): block)md"sv,
    .description = u8R"md(Inserts the table of contents at this location.
The table of contents is built from headings throughout the document.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior mark {
  HTML_Tag_Name(u8"mark"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"mark"sv,
    .declaration = u8R"md(mark(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<mark>` highlight element.)md"sv,
    .example = u8R"md(`mark{important}` → `<mark>important</mark>`)md"sv,
  },
};
constexpr Math_Behavior math {
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"math"sv,
    .declaration = u8R"md(math(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Renders content as an inline math expression (MathML).)md"sv,
    .example = u8R"md(`math{x^2 + y^2}` produces inline math.)md"sv,
  },
};
constexpr Math_Behavior mathblock {
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"mathblock"sv,
    .declaration = u8R"md(mathblock(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Renders content as a block-level math expression (MathML).)md"sv,
  },
};
constexpr In_Tag_Behavior nobr {
  html_tag::span,
  u8"word",
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"nobr"sv,
    .declaration = u8R"md(nobr(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in a `<span class="word">` to prevent line breaks within the content.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior noscript {
  HTML_Tag_Name(u8"noscript"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"noscript"sv,
    .declaration = u8R"md(noscript(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<noscript>` element,
shown when JavaScript is disabled.)md"sv,
  },
};
constexpr In_Tag_Behavior o {
  html_tag::span,
  u8"oblique",
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"o"sv,
    .declaration = u8R"md(o(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in a `<span class="oblique">` for oblique styling.)md"sv,
    .example = u8R"md(`o{hello}` → `<span class="oblique">hello</span>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior ol {
  html_tag::ol,
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"ol"sv,
    .declaration = u8R"md(ol(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<ol>` ordered list element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior p {
  HTML_Tag_Name(u8"p"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"p"sv,
    .declaration = u8R"md(p(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<p>` paragraph element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior pre {
  HTML_Tag_Name(u8"pre"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"pre"sv,
    .declaration = u8R"md(pre(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<pre>` preformatted text element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior q {
  HTML_Tag_Name(u8"q"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"q"sv,
    .declaration = u8R"md(q(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<q>` inline quotation element.)md"sv,
    .example = u8R"md(`q{hello}` → `<q>hello</q>`)md"sv,
  },
};
constexpr Ref_Behavior ref {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"ref"sv,
    .declaration = u8R"md(ref(
  to: str,
  content?: block,
): block)md"sv,
    .description = u8R"md(Creates a hyperlink or document reference.
`to` is the target URL or anchor;
`content` is the visible link content (auto-generated if not provided).)md"sv,
    .example = u8R"md(`ref("#sec1"){See section 1}` → `<a href="#sec1">See section 1</a>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior s {
  HTML_Tag_Name(u8"s"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"s"sv,
    .declaration = u8R"md(s(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<s>` strikethrough element.)md"sv,
    .example = u8R"md(`s{wrong}` → `<s>wrong</s>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior samp {
  HTML_Tag_Name(u8"samp"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"samp"sv,
    .declaration = u8R"md(samp(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<samp>` sample output element.)md"sv,
    .example = u8R"md(`samp{error: file not found}` → `<samp>error: file not found</samp>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior sans {
  HTML_Tag_Name(u8"f-sans"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"sans"sv,
    .declaration = u8R"md(sans(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an `<f-sans>` element for sans-serif styling.)md"sv,
    .example = u8R"md(`sans{hello}` → `<f-sans>hello</f-sans>`)md"sv,
  },
};
constexpr HTML_Raw_Text_Behavior script {
  html_tag::script,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"script"sv,
    .declaration = u8R"md(script(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Embeds raw content as an HTML `<script>` element.
Content is taken literally (not HTML-escaped).)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior serif {
  HTML_Tag_Name(u8"f-serif"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"serif"sv,
    .declaration = u8R"md(serif(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an `<f-serif>` element for serif styling.)md"sv,
    .example = u8R"md(`serif{hello}` → `<f-serif>hello</f-serif>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior small {
  HTML_Tag_Name(u8"small"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"small"sv,
    .declaration = u8R"md(small(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<small>` element for smaller text.)md"sv,
    .example = u8R"md(`small{note}` → `<small>note</small>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior span {
  html_tag::span,
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"span"sv,
    .declaration = u8R"md(span(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<span>` inline container element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior strong {
  HTML_Tag_Name(u8"strong"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"strong"sv,
    .declaration = u8R"md(strong(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<strong>` element for strong emphasis.)md"sv,
    .example = u8R"md(`strong{important}` → `<strong>important</strong>`)md"sv,
  },
};
constexpr HTML_Raw_Text_Behavior style {
  html_tag::style,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"style"sv,
    .declaration = u8R"md(style(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Embeds raw content as an HTML `<style>` element.
Content is taken literally (not HTML-escaped).)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior sub {
  HTML_Tag_Name(u8"sub"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"sub"sv,
    .declaration = u8R"md(sub(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<sub>` subscript element.)md"sv,
    .example = u8R"md(`sub{2}` → `<sub>2</sub>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior summary {
  HTML_Tag_Name(u8"summary"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"summary"sv,
    .declaration = u8R"md(summary(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<summary>` element, used inside `details`.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior sup {
  HTML_Tag_Name(u8"sup"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"sup"sv,
    .declaration = u8R"md(sup(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<sup>` superscript element.)md"sv,
    .example = u8R"md(`sup{2}` → `<sup>2</sup>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior table {
  HTML_Tag_Name(u8"table"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"table"sv,
    .declaration = u8R"md(table(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<table>` element.)md"sv,
  },
};
constexpr URL_Behavior tel {
  u8"tel:",
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"tel"sv,
    .declaration = u8R"md(tel(content: block): block)md"sv,
    .description = u8R"md(Creates a `tel:` hyperlink where the content is the phone number.)md"sv,
    .example = u8R"md(`tel{+1-800-555-1234}` → `<a href="tel:+1-800-555-1234">+1-800-555-1234</a>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior tbody {
  HTML_Tag_Name(u8"tbody"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"tbody"sv,
    .declaration = u8R"md(tbody(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<tbody>` table body element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior td {
  HTML_Tag_Name(u8"td"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"td"sv,
    .declaration = u8R"md(td(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<td>` table data cell element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior tfoot {
  HTML_Tag_Name(u8"tfoot"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"tfoot"sv,
    .declaration = u8R"md(tfoot(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<tfoot>` table footer element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior th {
  HTML_Tag_Name(u8"th"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"th"sv,
    .declaration = u8R"md(th(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<th>` table header cell element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior thead {
  HTML_Tag_Name(u8"thead"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"thead"sv,
    .declaration = u8R"md(thead(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<thead>` table header element.)md"sv,
  },
};
constexpr There_Behavior there {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"there"sv,
    .declaration = u8R"md(there(
  section: str,
  content: block,
): block)md"sv,
    .description = u8R"md(Stashes `content` into the named section.
The content is later inserted by `here` or `hereblock` with the same section name.)md"sv,
    .example = u8R"md(`there(section: toc){...}` stashes content for insertion at `here(section: toc)`.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior tr {
  HTML_Tag_Name(u8"tr"),
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"tr"sv,
    .declaration = u8R"md(tr(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<tr>` table row element.)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior tt {
  HTML_Tag_Name(u8"tt-"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"tt"sv,
    .declaration = u8R"md(tt(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in a `<tt->` element for teletype/monospace styling.)md"sv,
    .example = u8R"md(`tt{hello}` → `<tt->hello</tt->`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior u {
  HTML_Tag_Name(u8"u"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"u"sv,
    .declaration = u8R"md(u(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<u>` underline element.)md"sv,
    .example = u8R"md(`u{hello}` → `<u>hello</u>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior ul {
  html_tag::ul,
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"ul"sv,
    .declaration = u8R"md(ul(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<ul>` unordered list element.)md"sv,
  },
};
constexpr URL_Behavior url {
  u8"",
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"url"sv,
    .declaration = u8R"md(url(content: block): block)md"sv,
    .description = u8R"md(Creates a hyperlink where the content is the URL and serves as both href and label.)md"sv,
    .example = u8R"md(`url{https://example.com}` → `<a href="https://example.com">https://example.com</a>`)md"sv,
  },
};
constexpr Fixed_Name_Passthrough_Behavior var {
  HTML_Tag_Name(u8"var"),
  Policy_Usage::inherit,
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"var"sv,
    .declaration = u8R"md(var(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in an HTML `<var>` variable element.)md"sv,
    .example = u8R"md(`var{x}` → `<var>x</var>`)md"sv,
  },
};
constexpr Self_Closing_Behavior wbr {
  HTML_Tag_Name(u8"wbr"),
  Directive_Display::in_line,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"wbr"sv,
    .declaration = u8R"md(wbr(): block)md"sv,
    .description = u8R"md(Inserts an HTML `<wbr>` word break opportunity.)md"sv,
  },
};
constexpr In_Tag_Behavior wg21_grammar {
  HTML_Tag_Name(u8"dl"),
  u8"grammar",
  Policy_Usage::html,
  Directive_Display::block,
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"wg21_grammar"sv,
    .declaration = u8R"md(wg21_grammar(
  attr: pack named str,
  content: block,
): block)md"sv,
    .description = u8R"md(Wraps content in a `<dl class="grammar">` element for WG21 grammar notation.)md"sv,
  },
};
constexpr WG21_Head_Behavior wg21_head {
  Tooltip_Article {
    .kind = Tooltip_Kind::builtin_directive,
    .subject = u8"wg21_head"sv,
    .declaration = u8R"md(wg21_head(
  title: block,
  content: block,
): block)md"sv,
    .description = u8R"md(Generates a WG21-style document header `<div class="wg21-head">`.
`title` is the document title; `content` is the body of the header.)md"sv,
  },
};
// clang-format on

// clang-format off
#define COWEL_DEPRECATED_ALIAS(name, use_instead)                          \
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
    COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_str_at),
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

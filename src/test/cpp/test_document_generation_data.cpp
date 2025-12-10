#include "cowel/diagnostic.hpp"

#include "test_data.hpp"

namespace cowel {
namespace {

// Note that we are defining this array in a separate file because it gets updated regularly,
// and recompiling test_document_generation.cpp requires an awful lot of time,
// so we'd much rather have this small cpp file that is fast to compile (incrementally).

// clang-format off
constexpr Basic_Test basic_tests_array[] {
    { Source {u8"\\cowel_char_by_entity{#x41}\\cowel_char_by_entity{#x42}\\cowel_char_by_entity{#x43}\n"},
      Source{ u8"ABC\n" } },

    { Source{ u8"\\cowel_char_by_entity{#x00B6}\n" },
      Source { u8"\N{PILCROW SIGN}\n" } },

    { Source{ u8"\\cowel_char_by_entity{}\n" },
      Source { u8"<error->\\cowel_char_by_entity{}</error->\n" },
      Processing_Status::error,
      { diagnostic::char_blank } },

    { Source{ u8"\\cowel_char_by_entity{ }\n" },
      Source { u8"<error->\\cowel_char_by_entity{ }</error->\n" },
      Processing_Status::error,
      { diagnostic::char_blank } },

    { Source{ u8"\\cowel_char_by_entity{#zzz}\n" },
      Source { u8"<error->\\cowel_char_by_entity{#zzz}</error->\n" },
      Processing_Status::error,
      { diagnostic::char_digits } },

    { Source{ u8"\\cowel_char_by_entity{#xD800}\n" },
      Source { u8"<error->\\cowel_char_by_entity{#xD800}</error->\n" },
      Processing_Status::error,
      { diagnostic::char_nonscalar } },
    
    { Path { u8"U/ascii.cow" },
      Source { u8"ABC\n" } },

    { Source { u8"\\cowel_char_by_num{00B6}\n" },
      Source { u8"¶\n" } },

    { Source { u8"\\cowel_char_by_num{}\n" },
      Source { u8"<error->\\cowel_char_by_num{}</error->\n" },
      Processing_Status::error,
      { diagnostic::char_blank } },

    { Source { u8"\\cowel_char_by_num{ }\n" },
      Source { u8"<error->\\cowel_char_by_num{ }</error->\n" },
      Processing_Status::error,
      { diagnostic::char_blank } },

    { Source { u8"\\cowel_char_by_num{zzz}\n" },
      Source { u8"<error->\\cowel_char_by_num{zzz}</error->\n" },
      Processing_Status::error,
      { diagnostic::char_digits } },

    { Source { u8"\\cowel_char_by_num{D800}\n" },
      Source { u8"<error->\\cowel_char_by_num{D800}</error->\n" },
      Processing_Status::error,
      { diagnostic::char_nonscalar } },

    { Source { u8"\\cowel_invoke(cowel_char_by_num){00B6}\n" },
      Source { u8"¶\n" } },

    { Source { u8"\\cowel_invoke(cowel_char_by_num){ }\n" },
      Source { u8"<error->\\cowel_invoke(cowel_char_by_num){ }</error->\n" },
      Processing_Status::error,
      { diagnostic::char_blank } },

    { Source { u8"\\cowel_invoke\n" },
      Source { u8"<error->\\cowel_invoke</error->\n" },
      Processing_Status::error,
      { diagnostic::type_mismatch } },

    { Source { u8"\\cowel_invoke(\"???\")\n" },
      Source { u8"<error->\\cowel_invoke(\"???\")</error->\n" },
      Processing_Status::error,
      { diagnostic::invoke_name_invalid } },

    { Source { u8"\\cowel_invoke(\"???\")\n" },
      Source { u8"<error->\\cowel_invoke(\"???\")</error->\n" },
      Processing_Status::error,
      { diagnostic::invoke_name_invalid } },

    { Path { u8"alias.cow" },
      Path { u8"alias.cow.html" } },

    { Source { u8".\\cowel_alias{\\undefined}\n" },
      Source { u8"." },
      Processing_Status::fatal,
      { diagnostic::alias_name_invalid } },

    { Source { u8".\\cowel_alias{??}\n" },
      Source { u8"." },
      Processing_Status::fatal,
      { diagnostic::alias_name_invalid } },

    { Source { u8".\\cowel_alias(\"?\"){cowel_alias}\n" },
      Source { u8"." },
      Processing_Status::fatal,
      { diagnostic::alias_name_invalid } },

    { Source { u8".\\cowel_alias(a, a){cowel_alias}\n" },
      Source { u8"." },
      Processing_Status::fatal,
      { diagnostic::alias_duplicate } },

    { Source { u8"\\url{https://cowel.org}" },
      Source { u8"<a href=https://cowel.org class=sans>https://cowel.org</a>" } },

    { Source { u8"\\h1{Heading}\n" },
      Source { u8"<h1 id=heading><a class=para href=#heading></a>Heading</h1>\n" } },

    { Source { u8"\\h1{\\code(x){abcx}}\n" },
      Source { u8"<h1 id=abcx><a class=para href=#abcx></a><code>abc<h- data-h=kw>x</h-></code></h1>\n" } },

    { Source { u8"\\h2(listed=false){ }\n" },
      Source { u8"<h2> </h2>\n" } },

    { Source { u8"\\h3(id=\"user id\",listed=false){Heading}\n" },
      Source { u8"<h3 id=\"user id\"><a class=para href=\"#user%20id\"></a>Heading</h3>\n" } },

    { Source { u8"\\h4(id=user-id,listed=false){Heading}\n" },
      Source { u8"<h4 id=user-id><a class=para href=#user-id></a>Heading</h4>\n" } },

    { Source { u8"\\style{b { color: red; }}\n" },
      Source { u8"<style>b { color: red; }</style>\n" } },
    
    { Source { u8"\\script{let x = 3 < 5; let y = true && false;}\n" },
      Source { u8"<script>let x = 3 < 5; let y = true && false;</script>\n" } },

    { Source { u8"\\script{</script>}" },
      Source { u8"<script></script>" },
      Processing_Status::error,
      { diagnostic::raw_text_closing } },
    
    { Source { u8"\\style{</style>}" },
      Source { u8"<style></style>" },
      Processing_Status::error,
      { diagnostic::raw_text_closing } },

    { Source { u8"\\code{}\n" },
      Source { u8"<error->\\code{}</error->\n" },
      Processing_Status::error,
      { diagnostic::type_mismatch } },

    { Source { u8"\\code(x){}\n" },
      Source { u8"<code></code>\n" } },

    { Source { u8"\\code(x){ }\n" },
      Source { u8"<code> </code>\n" } },

    { Source { u8"\\code(x){xxx}\n" },
      Source { u8"<code><h- data-h=kw>xxx</h-></code>\n" } },

    { Source { u8"\\code(x){xxx123}\n" },
      Source { u8"<code><h- data-h=kw>xxx</h->123</code>\n" } },

    { Source { u8"\\code(x){ 123 }\n" },
      Source { u8"<code> 123 </code>\n" } },

    { Source { u8"\\code(x){ \\b{123} }\n" },
      Source { u8"<code> <b>123</b> </code>\n" } },

    { Source { u8"\\code(x){ \\b{xxx} }\n" },
      Source { u8"<code> <b><h- data-h=kw>xxx</h-></b> </code>\n" } },

    { Source { u8"\\code(x){ \\b{x}xx }\n" },
      Source { u8"<code> <b><h- data-h=kw>x</h-></b><h- data-h=kw>xx</h-> </code>\n" } },

    { Path { u8"codeblock/trim.cow" },
      Path { u8"codeblock/trim.html" } },

    { Source { u8"\\cowel_highlight_as(keyword){awoo}\n" },
      Source { u8"<h- data-h=kw>awoo</h->\n" } },

    { Source { u8"\\code(c){int \\cowel_highlight_as(number){x}}\n" },
      Source { u8"<code><h- data-h=kw_type>int</h-> <h- data-h=num>x</h-></code>\n" } },

    { Source { u8"\\math{\\mi(id=Z){x}}\n" },
      Source { u8"<math display=inline><mi id=Z>x</mi></math>\n" } },

    { Path { u8"macro/new.cow" },
      Path { u8"macro/new.cow.html" } },

    { Path { u8"macro/multiline.cow" },
      Path { u8"macro/multiline.cow.html" } },

    { Path { u8"macro/forwarding_positional.cow" },
      Path { u8"macro/forwarding_positional.cow.html" } },

    { Path { u8"macro/forwarding_named.cow" },
      Path { u8"macro/forwarding_named.cow.html" } },

    { Path { u8"macro/put_paragraphs.cow" },
      Path { u8"macro/put_paragraphs.cow.html" } },

    { Source { u8"\\cowel_macro(content){\\cowel_put}\\content{Content}\n" },
      Source { u8"Content\n" } },

    { Source { u8"\\cowel_macro(pos){\\cowel_put{0}}\\pos(Positional)\n" },
      Source { u8"Positional\n" } },

    { Source { u8"\\cowel_macro(named){\\cowel_put{n}}\\named(n = Named)\n" },
      Source { u8"Named\n" } },

    { Source { u8"\\cowel_macro(try){\\cowel_put(else=Failure){0}}\\try(Success) \\try\n" },
      Source { u8"Success Failure\n" } },

    { Source { u8"\\cowel_macro(m){\\cowel_put{greeting}, \\cowel_put\\cowel_put{0}}"
               u8"\\m(greeting = Hello, \"!\"){macros}\n" },
      Source { u8"Hello, macros!\n" } },

    { Source { u8"\\cowel_macro(nested){\\cowel_put{\\cowel_put}}\\nested(X){0}\n" },
      Source { u8"X\n" } },

    { Source { u8"\\cowel_put\n" },
      Source { u8"<error->\\cowel_put</error->\n" },
      Processing_Status::error,
      { diagnostic::put_outside } },

    { Source { u8"\\awoo\n" },
      Source { u8"<error->\\awoo</error->\n" },
      Processing_Status::error,
      { diagnostic::directive_lookup_unresolved } },

    { Source { u8"\\code(x){\\awoo}\n" },
      Source { u8"<code><error->\\awoo</error-></code>\n" },
      Processing_Status::error,
      { diagnostic::directive_lookup_unresolved } },

    { Source { u8"\\cowel_html_element(div)" },
      Source { u8"<div></div>" } },

    { Source { u8"\\cowel_html_element(span, (id=abc)){span content}" },
      Source { u8"<span id=abc>span content</span>" },
      Processing_Status::ok },

    { Source { u8"\\cowel_html_element(span, (id=abc, x)){span content}" },
      Source { u8"<error->\\cowel_html_element(span, (id=abc, x)){span content}</error->" },
      Processing_Status::error,
      { diagnostic::type_mismatch } },

    { Source { u8"\\cowel_html_element" },
      Source { u8"<error->\\cowel_html_element</error->" },
      Processing_Status::error,
      { diagnostic::type_mismatch } },

    { Source { u8"\\cowel_html_element(\"<\")" },
      Source { u8"<error->\\cowel_html_element(\"&lt;\")</error->" },
      Processing_Status::error,
      { diagnostic::html_element_name_invalid } },

    { Source { u8"\\cowel_html_self_closing_element(hr)" },
      Source { u8"<hr/>" } },

    { Source { u8"\\cowel_html_self_closing_element(hr, (id=abc))" },
      Source { u8"<hr id=abc />" },
      Processing_Status::ok },

    { Source { u8"\\cowel_html_self_closing_element(hr, (id=abc, x))" },
      Source { u8"<error->\\cowel_html_self_closing_element(hr, (id=abc, x))</error->" },
      Processing_Status::error,
      { diagnostic::type_mismatch } },

    { Source { u8"\\cowel_html_self_closing_element" },
      Source { u8"<error->\\cowel_html_self_closing_element</error->" },
      Processing_Status::error,
      { diagnostic::type_mismatch } },

    { Source { u8"\\cowel_html_self_closing_element(\"<\")" },
      Source { u8"<error->\\cowel_html_self_closing_element(\"&lt;\")</error->" },
      Processing_Status::error,
      { diagnostic::html_element_name_invalid } },

    { Source { u8"\\cowel_div(1, 0)" },
      Source { u8"<error->\\cowel_div(1, 0)</error->" },
      Processing_Status::error,
      { diagnostic::type_mismatch } },

    { Source { u8"\\cowel_div_to_zero(1, 0)" },
      Source { u8"<error->\\cowel_div_to_zero(1, 0)</error->" },
      Processing_Status::error,
      { diagnostic::arithmetic_div_by_zero } },

    { Source { u8"\\cowel_pos(1e10000)" },
      Source { u8"infinity" },
      Processing_Status::ok,
      { diagnostic::literal_out_of_range } },

    { Source { u8"\\cowel_pos(-1e10000)" },
      Source { u8"-infinity" },
      Processing_Status::ok,
      { diagnostic::literal_out_of_range } },

    { Path { u8"splice/floats.cow" },
      Path { u8"splice/floats.cow.html" } },

    { Source { u8"" },
      Path { u8"document/empty.html" },
      Processing_Status::ok,
      {},
      Test_Behavior::empty_head },
    
    { Path { u8"empty.cow" },
      Source { u8"" } },
    
    { Path { u8"text.cow" },
      Source { u8"Hello, world!\n" } },

    { Path { u8"highlight.cow" },
      Path { u8"highlight.cow.html" } },
    
    { Path { u8"comments.cow" },
      Path { u8"comments.cow.html" } },

    { Path { u8"arithmetic/basic.cow" },
      Path { u8"arithmetic/basic.cow.html" } },

    { Path { u8"arithmetic/min_max.cow" },
      Path { u8"arithmetic/min_max.cow.html" } },

    { Path { u8"logical/ops.cow" },
      Path { u8"logical/ops.cow.html" } },

    { Path { u8"logical/short_circuit.cow" },
      Path { u8"logical/short_circuit.cow.html" } },

    { Path { u8"policy/no_invoke.cow" },
      Path { u8"policy/no_invoke.cow.html" } },

    { Path { u8"policy/paragraphs.cow" },
      Path { u8"policy/paragraphs.cow.html" } },

    { Path { u8"policy/source_as_text.cow" },
      Path { u8"policy/source_as_text.cow.html" } },

    { Path { u8"policy/highlight.cow" },
      Path { u8"policy/highlight.cow.html" } },

    { Path { u8"policy/text_as_html.cow" },
      Path { u8"policy/text_as_html.cow.html" } },

    { Path { u8"policy/text_only.cow" },
      Path { u8"policy/text_only.cow.html" } },

    { Path { u8"policy/to_html.cow" },
      Path { u8"policy/to_html.cow.html" } },
    
    { .document = Path { u8"paragraphs.cow" },
      .expected_html = Path { u8"paragraphs.cow.html" },
      .behavior = Test_Behavior::paragraphs },

    { .document = Path { u8"paragraphs_deep.cow" },
      .expected_html = Path { u8"paragraphs_deep.cow.html" },
      .behavior = Test_Behavior::paragraphs },

    { .document = Path { u8"paragraphs_with_comments.cow" },
      .expected_html = Path { u8"paragraphs_with_comments.cow.html" },
      .behavior = Test_Behavior::paragraphs },
    
    { .document = Path { u8"paragraph_control.cow" },
      .expected_html = Path { u8"paragraph_control.cow.html" },
      .behavior = Test_Behavior::paragraphs },

    { .document = Path { u8"../docs/index.cow" },
      .expected_html = Path { u8"../docs/index.html" },
      .expected_status = Processing_Status::ok,
      .behavior = Test_Behavior::wg21 },
};
// clang-format on
} // namespace

constinit const std::span<const Basic_Test> basic_tests { basic_tests_array };

} // namespace cowel

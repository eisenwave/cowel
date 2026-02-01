#ifndef COWEL_INTEGRATION_TESTING_HPP
#define COWEL_INTEGRATION_TESTING_HPP

#include <string_view>

namespace cowel {

constexpr std::u8string_view integration_test_preamble = u8R"!(
\cowel_macro(test_input){\cowel_var_let(__test_input,"\cowel_as_text{\cowel_to_html{\cowel_put}}")}
\cowel_macro(test_output){\cowel_var_let(__test_output,"\__arg_source_as_text(cowel_put())")}
\cowel_alias(test_expect_warning){__expect_warning}
\cowel_alias(test_expect_error){__expect_error}
\cowel_alias(test_expect_fatal){__expect_fatal}
)!";

}

#endif

#include <iostream>
#include <memory_resource>
#include <string_view>
#include <vector>

#include "mmml/util/annotated_string.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/io.hpp"
#include "mmml/util/result.hpp"
#include "mmml/util/tty.hpp"

#include "mmml/fwd.hpp"
#include "mmml/parse.hpp"
#include "mmml/print.hpp"

#include "compilation_stage.hpp"
#include "diagnostic_policy.hpp"
#include "document_file_testing.hpp"

namespace mmml {
namespace {

using Suppress_Unused_Include_Annotated_String = Basic_Annotated_String<void, void>;

struct Printing_Diagnostic_Policy : Diagnostic_Policy {
    std::string_view file;
    std::u8string_view source;
};

bool test_validity(std::string_view file, Printing_Diagnostic_Policy& policy)
{
#define MMML_SWITCH_ON_POLICY_ACTION(...)                                                          \
    switch (__VA_ARGS__) {                                                                         \
    case Policy_Action::success: return true;                                                      \
    case Policy_Action::failure: return false;                                                     \
    case Policy_Action::keep_going: break;                                                         \
    }
    std::pmr::monotonic_buffer_resource memory;

    const auto full_path = "test/" + std::pmr::string { file, &memory };
    policy.file = full_path;

    std::pmr::vector<char8_t> source_data { &memory };
    if (Result<void, IO_Error_Code> r = load_utf8_file(source_data, full_path); !r) {
        return policy.error(r.error()) == Policy_Action::success;
    }
    MMML_SWITCH_ON_POLICY_ACTION(policy.done(Compilation_Stage::load_file));
    const std::u8string_view source { source_data.data(), source_data.size() };
    policy.source = source;

    auto doc = parse_and_build(source, &memory);
    MMML_SWITCH_ON_POLICY_ACTION(policy.done(Compilation_Stage::parse));

// FIXME reimplement
#if 0 // NOLINT(readability-avoid-unconditional-preprocessor-if)
    Ignoring_HTML_Token_Consumer consumer;
    Result<void, bmd::Document_Error> result
        = bmd::doc_to_html(consumer, *doc, { .indent_width = 4 }, &memory);
    if (!result) {
        MMML_SWITCH_ON_POLICY_ACTION(policy.error(result.error()));
    }
    MMML_SWITCH_ON_POLICY_ACTION(policy.done(Compilation_Stage::process));

    return policy.is_success();
#endif
    return true;
}

struct Expect_Success_Diagnostic_Policy final : Printing_Diagnostic_Policy {
private:
    Policy_Action m_action = Policy_Action::keep_going;

public:
    [[nodiscard]]
    bool is_success() const final
    {
        return m_action == Policy_Action::success;
    }

    Policy_Action error(IO_Error_Code e) final
    {
        Diagnostic_String out;
        print_io_error(out, file, e);
        print_code_string(std::cout, out, is_stdout_tty);
        return m_action = Policy_Action::failure;
    }

    Policy_Action done(Compilation_Stage stage) final
    {
        if (stage < Compilation_Stage::process) {
            return Policy_Action::keep_going;
        }
        return m_action = Policy_Action::success;
    }
};

} // namespace

bool test_for_success(std::string_view file, Compilation_Stage until_stage)
{
    // Sorry, testing for only partial success is not implemented yet.
    MMML_ASSERT(until_stage == Compilation_Stage::process);

    Expect_Success_Diagnostic_Policy policy;
    return test_validity(file, policy);
}

} // namespace mmml

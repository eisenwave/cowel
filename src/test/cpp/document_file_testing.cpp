#include <functional>
#include <iostream>
#include <memory_resource>

#include "mmml/annotated_string.hpp"
#include "mmml/diagnostics.hpp"
#include "mmml/io.hpp"
#include "mmml/parse.hpp"
#include "mmml/tty.hpp"

#include "diagnostic_policy.hpp"

namespace mmml {
namespace {

const bool should_print_colors = is_tty(stdout);

struct Printing_Diagnostic_Policy : Diagnostic_Policy {
    std::string_view file;
    std::string_view source;
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

    std::pmr::vector<char> source_data { &memory };
    if (Result<void, IO_Error_Code> r = file_to_bytes(source_data, full_path); !r) {
        return policy.error(r.error()) == Policy_Action::success;
    }
    MMML_SWITCH_ON_POLICY_ACTION(policy.done(Compilation_Stage::load_file));
    const std::string_view source { source_data.data(), source_data.size() };
    policy.source = source;

    auto doc = parse_and_build(source, &memory);
    MMML_SWITCH_ON_POLICY_ACTION(policy.done(Compilation_Stage::parse));

// FIXME reimplement
#if 0
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
    virtual bool is_success() const
    {
        return m_action == Policy_Action::success;
    }

    Policy_Action error(IO_Error_Code e) final
    {
        Annotated_String out;
        print_io_error(out, file, e);
        print_code_string(std::cout, out, should_print_colors);
        return m_action = Policy_Action::failure;
    }

    Policy_Action done(Compilation_Stage stage)
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

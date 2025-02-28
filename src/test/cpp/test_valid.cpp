#include <gtest/gtest.h>

#include "document_file_testing.hpp"

namespace mmml {
namespace {

TEST(Valid_BMD, empty)
{
    EXPECT_TRUE(test_for_success("empty.bmd"));
}

TEST(Valid_BMD, hello_code)
{
    EXPECT_TRUE(test_for_success("hello_code.bmd"));
}

TEST(Valid_BMD, hello_directive)
{
    EXPECT_TRUE(test_for_success("hello_directive.bmd"));
}

TEST(Valid_BMD, hello_world)
{
    EXPECT_TRUE(test_for_success("hello_world.bmd"));
}

} // namespace
} // namespace mmml

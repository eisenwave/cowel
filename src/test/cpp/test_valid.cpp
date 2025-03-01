#include <gtest/gtest.h>

#include "document_file_testing.hpp"

namespace mmml {
namespace {

TEST(Valid, empty)
{
    EXPECT_TRUE(test_for_success("empty.mmml"));
}

TEST(Valid, hello_code)
{
    EXPECT_TRUE(test_for_success("hello_code.mmml"));
}

TEST(Valid, hello_directive)
{
    EXPECT_TRUE(test_for_success("hello_directive.mmml"));
}

TEST(Valid, hello_world)
{
    EXPECT_TRUE(test_for_success("hello_world.mmml"));
}

} // namespace
} // namespace mmml

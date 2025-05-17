#include <gtest/gtest.h>

#include "document_file_testing.hpp"

namespace cowel {
namespace {

TEST(Valid, empty)
{
    EXPECT_TRUE(test_for_success("empty.cow"));
}

TEST(Valid, hello_code)
{
    EXPECT_TRUE(test_for_success("hello_code.cow"));
}

TEST(Valid, hello_directive)
{
    EXPECT_TRUE(test_for_success("hello_directive.cow"));
}

TEST(Valid, hello_world)
{
    EXPECT_TRUE(test_for_success("hello_world.cow"));
}

} // namespace
} // namespace cowel

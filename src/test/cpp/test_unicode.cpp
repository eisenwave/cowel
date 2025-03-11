#include <algorithm>
#include <iterator>
#include <memory_resource>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "mmml/util/io.hpp"
#include "mmml/util/result.hpp"
#include "mmml/util/unicode.hpp"

namespace mmml::utf8 {
namespace {

[[nodiscard]]
std::pmr::vector<char32_t> to_utf32(std::u8string_view utf8, std::pmr::memory_resource* memory)
{
    std::pmr::vector<char32_t> result { memory };
    std::ranges::copy(Code_Point_View { utf8 }, std::back_inserter(result));
    return result;
}

TEST(Unicode, sequence_length)
{
    // https://en.wikipedia.org/wiki/UTF-8
    EXPECT_EQ(sequence_length(0b0000'0000), 1);
    EXPECT_EQ(sequence_length(0b1000'0000), 0);
    EXPECT_EQ(sequence_length(0b1100'0000), 2);
    EXPECT_EQ(sequence_length(0b1110'0000), 3);
    EXPECT_EQ(sequence_length(0b1111'0000), 4);
    EXPECT_EQ(sequence_length(0b1111'1000), 0);
}

TEST(Unicode, decode_unchecked)
{
    EXPECT_EQ(decode_unchecked(u8"a"), U'a');
    EXPECT_EQ(decode_unchecked(u8"\u00E9"), U'\u00E9');
    EXPECT_EQ(decode_unchecked(u8"\u0905"), U'\u0905');
    EXPECT_EQ(decode_unchecked(u8"\U0001F600"), U'\U0001F600');
}

TEST(Unicode, decode_file)
{
    std::pmr::monotonic_buffer_resource memory;

    Result<std::pmr::vector<char8_t>, IO_Error_Code> utf8
        = load_utf8_file("test/utf8.txt", &memory);
    ASSERT_TRUE(utf8);

    Result<std::pmr::vector<char32_t>, IO_Error_Code> expected
        = load_utf32le_file("test/utf32le.txt", &memory);
    ASSERT_TRUE(expected);

    const std::u8string_view u8view { utf8->data(), utf8->size() };

    std::pmr::vector<char32_t> actual = to_utf32(u8view, &memory);

    ASSERT_EQ(*expected, actual);
}

} // namespace
} // namespace mmml::utf8

#include <memory_resource>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cowel/theme_to_css.hpp"

namespace cowel {
namespace {

TEST(Theme_To_Css, resolves_palette_references)
{
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> actual { &memory };
    constexpr auto theme = u8R"JSON(
{
    "palette": {
        "base": "#123456",
        "accent": "$base",
        "highlight": "$accent"
    },
    "light": {
        "comment": {
            "color": "$highlight",
            "font-style": "italic"
        },
        "keyword": "red"
    },
    "dark": {
        "comment": "$base"
    }
}
)JSON";

    ASSERT_TRUE(theme_to_css(actual, theme, &memory));
    const std::u8string_view actual_view { actual.data(), actual.size() };
    EXPECT_EQ(actual_view, u8R"CSS(@media (prefers-color-scheme: light) {
h-[data-h^=cmt] { color:#123456; font-style:italic; }
h-[data-h^=kw] { color:red; }
}
@media (prefers-color-scheme: dark) {
h-[data-h^=cmt] { color:#123456; }
}

html.light h-[data-h^=cmt] { color:#123456; font-style:italic; }
html.light h-[data-h^=kw] { color:red; }

html.dark h-[data-h^=cmt] { color:#123456; }
)CSS");
}

TEST(Theme_To_Css, allows_themes_without_palette)
{
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> actual { &memory };
    constexpr auto theme = u8R"JSON(
{
    "light": {
        "comment": "#123456"
    },
    "dark": {
        "comment": {
            "color": "red",
            "font-style": "italic"
        }
    }
}
)JSON";

    ASSERT_TRUE(theme_to_css(actual, theme, &memory));
    const std::u8string_view actual_view { actual.data(), actual.size() };
    EXPECT_EQ(actual_view, u8R"CSS(@media (prefers-color-scheme: light) {
h-[data-h^=cmt] { color:#123456; }
}
@media (prefers-color-scheme: dark) {
h-[data-h^=cmt] { color:red; font-style:italic; }
}

html.light h-[data-h^=cmt] { color:#123456; }

html.dark h-[data-h^=cmt] { color:red; font-style:italic; }
)CSS");
}

TEST(Theme_To_Css, rejects_unresolved_palette_references)
{
    std::pmr::monotonic_buffer_resource memory;
    std::pmr::vector<char8_t> actual { &memory };
    constexpr auto theme = u8R"JSON(
{
    "palette": {
        "base": "$missing"
    },
    "light": {
        "comment": "$base"
    },
    "dark": {
        "comment": "#123456"
    }
}
)JSON";

    EXPECT_FALSE(theme_to_css(actual, theme, &memory));
    EXPECT_TRUE(actual.empty());
}

} // namespace
} // namespace cowel

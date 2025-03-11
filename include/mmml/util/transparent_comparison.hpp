#ifndef MMML_TRANSPARENT_COMPARISON_HPP
#define MMML_TRANSPARENT_COMPARISON_HPP

#include <cstddef>
#include <string_view>

namespace mmml {

template <typename Char>
struct Basic_Transparent_String_View_Hash {
    using is_transparent = void;
    using char_type = Char;
    using string_view_type = std::basic_string_view<Char>;

    [[nodiscard]]
    std::size_t operator()(string_view_type v) const
    {
        return std::hash<string_view_type> {}(v); // NOLINT(misc-include-cleaner)
    }
};

template <typename Char>
struct Basic_Transparent_String_View_Equals {
    using is_transparent = void;
    using char_type = Char;
    using string_view_type = std::basic_string_view<Char>;

    [[nodiscard]]
    bool operator()(string_view_type x, string_view_type y) const
    {
        return x == y;
    }
};

} // namespace mmml

#endif

#ifndef COWEL_VALUE_HPP
#define COWEL_VALUE_HPP

#include <string_view>
#include <vector>

#include "cowel/fwd.hpp"

namespace cowel {

struct Maybe_Owning_String {
private:
    std::pmr::vector<char8_t> m_data;
    std::u8string_view m_str;

public:
    Maybe_Owning_String(std::u8string_view str)
        : m_str { str }
    {
    }

    Maybe_Owning_String(std::pmr::vector<char8_t>&& str)
        : m_data { std::move(str) }
        , m_str { m_data.data(), m_data.size() }
    {
    }
};

struct Markup_And_Frame {
    const ast::Content_Sequence* markup;
    Frame_Index frame;
};

enum struct Type : Default_Underlying {
    boolean,
    integer,
    string,
    markup,
};

struct Value {
private:
    Type m_type;
    union {
        bool boolean;
        long long integer;
        Maybe_Owning_String string;
        Markup_And_Frame markup;
    };

public:
    [[nodiscard]]
    Value(bool value)
        : m_type { Type::boolean }
        , boolean { value }
    {
    }
    [[nodiscard]]
    Value(long long value)
        : m_type { Type::integer }
        , integer { value }
    {
    }
    [[nodiscard]]
    Value(Maybe_Owning_String&& value)
        : m_type { Type::string }
        , string { std::move(value) }
    {
    }
    [[nodiscard]]
    Value(Markup_And_Frame value)
        : m_type { Type::markup }
        , markup { value }
    {
    }

    [[nodiscard]]
    Type get_type() const
    {
        return m_type;
    }
};

} // namespace cowel

#endif

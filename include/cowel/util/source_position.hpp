#ifndef COWEL_SOURCE_POSITION_HPP
#define COWEL_SOURCE_POSITION_HPP

#include <cstddef>
#include <string_view>
#include <type_traits>

#include "cowel/util/assert.hpp"

#include "cowel/fwd.hpp"

namespace cowel {

/// Represents a position in a source file.
struct Source_Position {
    /// Line number.
    std::size_t line;
    /// Column number.
    std::size_t column;
    /// First index in the source file that is part of the syntactical element.
    std::size_t begin;

    [[nodiscard]]
    friend constexpr auto operator<=>(Source_Position, Source_Position)
        = default;

    [[nodiscard]]
    constexpr Source_Position to_right(std::size_t offset) const
    {
        return { .line = line, .column = column + offset, .begin = begin + offset };
    }

    [[nodiscard]]
    constexpr Source_Position to_left(std::size_t offset) const
    {
        COWEL_ASSERT(column >= offset);
        COWEL_ASSERT(begin >= offset);
        return { .line = line, .column = column - offset, .begin = begin - offset };
    }
};

constexpr void advance(Source_Position& pos, char8_t c)
{
    switch (c) {
    case '\r': pos.column = 0; break;
    case '\n':
        pos.column = 0;
        pos.line += 1;
        break;
    default: pos.column += 1;
    }
    pos.begin += 1;
}

constexpr void advance(Source_Position& pos, std::u8string_view str)
{
    for (const char8_t c : str) {
        advance(pos, c);
    }
}

/// Represents a position in a source file.
struct Source_Span : Source_Position {
    std::size_t length;

    [[nodiscard]]
    friend constexpr auto operator<=>(Source_Span, Source_Span)
        = default;

    /// @brief Returns a span with the same properties except that the length is `l`.
    [[nodiscard]]
    constexpr Source_Span with_length(std::size_t l) const
    {
        return { Source_Position { *this }, l }; // NOLINT(cppcoreguidelines-slicing)
    }

    /// @brief Returns a span on the same line and with the same length, shifted to the right
    /// by `offset` characters.
    [[nodiscard]]
    constexpr Source_Span to_right(std::size_t offset) const
    {
        return { { .line = line, .column = column + offset, .begin = begin + offset }, length };
    }

    /// @brief Returns a span on the same line and with the same length, shifted to the left by
    /// `offset` characters.
    /// The `offset` shall not be greater than `this->column` or `this->begin`
    /// (which would correspond to flowing off the beginning of the column or source).
    [[nodiscard]]
    constexpr Source_Span to_left(std::size_t offset) const
    {
        COWEL_ASSERT(column >= offset);
        COWEL_ASSERT(begin >= offset);
        return { { .line = line, .column = column - offset, .begin = begin - offset }, length };
    }

    [[nodiscard]]
    constexpr bool empty() const
    {
        return length == 0;
    }

    /// @brief Returns the one-past-the-end column.
    [[nodiscard]]
    constexpr std::size_t end_column() const
    {
        return column + length;
    }

    /// @brief Returns the one-past-the-end position in the source.
    [[nodiscard]]
    constexpr std::size_t end() const
    {
        return begin + length;
    }

    /// @brief Returns the one-past-the-end position as a `Local_Source_Position`.
    /// This position is assumed to be on the same line and one column past this span.
    [[nodiscard]]
    constexpr Source_Position end_pos() const
    {
        return { .line = line, .column = column + length, .begin = begin + length };
    }

    [[nodiscard]]
    constexpr bool contains(std::size_t pos) const
    {
        return pos >= begin && pos < end();
    }
};

template <typename File>
struct Basic_File_Source_Span : Source_Span {
    static_assert(std::is_trivially_copyable_v<File>);

    File file;

    [[nodiscard]]
    constexpr Basic_File_Source_Span(const Source_Span& local, File file)
        : Source_Span { local }
        , file { file }
    {
    }

    [[nodiscard]]
    constexpr Basic_File_Source_Span(const Source_Position& local, std::size_t length, File file)
        : Source_Span { local, length }
        , file { file }
    {
    }

    [[nodiscard]]
    friend constexpr auto operator<=>(Basic_File_Source_Span, Basic_File_Source_Span)
        = default;

    [[nodiscard]]
    constexpr std::size_t end() const
    {
        return begin + length;
    }

    /// @brief Returns a span with the same properties except that the length is `l`.
    [[nodiscard]]
    constexpr Basic_File_Source_Span with_length(std::size_t l) const
    {
        return { Source_Span::with_length(l), file };
    }

    [[nodiscard]]
    constexpr Basic_File_Source_Span to_right(std::size_t offset) const
    {
        return { Source_Span::to_right(offset), file };
    }

    [[nodiscard]]
    constexpr Basic_File_Source_Span to_left(std::size_t offset) const
    {
        return { Source_Span::to_left(offset), file };
    }
};

/// Represents the location of a file, combined with the position within that file.
template <typename File>
struct Basic_File_Source_Position : Source_Position {
    static_assert(std::is_trivially_copyable_v<File>);

    File file;

    [[nodiscard]]
    constexpr Basic_File_Source_Position(const Basic_File_Source_Span<File>& span)
        : Source_Position { span } // NOLINT(cppcoreguidelines-slicing)
        , file { span.file }
    {
    }

    [[nodiscard]]
    constexpr Basic_File_Source_Position(Source_Position local, File file)
        : Source_Position(local)
        , file { file }
    {
    }

    [[nodiscard]]
    friend constexpr auto operator<=>(Basic_File_Source_Position, Basic_File_Source_Position)
        = default;
};

} // namespace cowel

#endif

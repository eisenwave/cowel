#ifndef MMML_SOURCE_POSITION_HPP
#define MMML_SOURCE_POSITION_HPP

#include <cstddef>
#include <string_view>

#include "mmml/util/assert.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

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
        MMML_ASSERT(column >= offset);
        MMML_ASSERT(begin >= offset);
        return { .line = line, .column = column - offset, .begin = begin - offset };
    }
};

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
        MMML_ASSERT(column >= offset);
        MMML_ASSERT(begin >= offset);
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
};

/// Represents the location of a file, combined with the position within that file.
struct File_Source_Span : Source_Span {
    /// File name.
    std::string_view file_name;

    [[nodiscard]]
    constexpr File_Source_Span(Source_Span local, std::string_view file)
        : Source_Span(local)
        , file_name(file)
    {
    }

    [[nodiscard]]
    friend constexpr auto operator<=>(File_Source_Span, File_Source_Span)
        = default;

    [[nodiscard]]
    constexpr std::size_t end() const
    {
        return begin + length;
    }
};

/// Represents the location of a file, combined with the position within that file.
struct File_Source_Position : Source_Position {
    /// File name.
    std::string_view file_name;

    [[nodiscard]]
    constexpr File_Source_Position(const File_Source_Span& span)
        : Source_Position { span } // NOLINT(cppcoreguidelines-slicing)
        , file_name(span.file_name)
    {
    }

    [[nodiscard]]
    constexpr File_Source_Position(Source_Position local, std::string_view file)
        : Source_Position(local)
        , file_name(file)
    {
    }

    [[nodiscard]]
    friend constexpr auto operator<=>(File_Source_Position, File_Source_Position)
        = default;
};

} // namespace mmml

#endif

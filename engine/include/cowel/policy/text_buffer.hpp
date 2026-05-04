#ifndef COWEL_TEXT_BUFFER_HPP
#define COWEL_TEXT_BUFFER_HPP

#include <cstddef>

#include "cowel/util/assert.hpp"
#include "cowel/util/buffer.hpp"
#include "cowel/util/char_sequence.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/output_language.hpp"
#include "cowel/settings.hpp"

namespace cowel {

struct Text_Buffer_Sink {
    Text_Sink& parent;
    Output_Language language;

    constexpr void operator()(std::span<const char8_t> data)
    {
        parent.write(as_u8string_view(data), language);
    }
};

template <std::size_t cap>
    requires(cap != 0)
struct Text_Buffer final
    : Text_Sink
    , Buffer<char8_t, cap, Text_Buffer_Sink> {

    [[nodiscard]]
    constexpr Text_Buffer(Text_Sink& parent, Output_Language language) noexcept
        : Text_Sink { language }
        , Buffer<char8_t, cap, Text_Buffer_Sink> { Text_Buffer_Sink { parent, language } }
    {
    }

    COWEL_HOT
    bool write(Char_Sequence8 chars, [[maybe_unused]] Output_Language lang) final
    {
        COWEL_DEBUG_ASSERT(lang == get_language());

        if (chars.empty()) {
            return true;
        }
        if (const std::u8string_view sv = chars.as_string_view(); !sv.empty()) {
            this->append_range(sv);
            return true;
        }
        this->append_in_place(chars.size(), [&](std::span<char8_t> out) {
            COWEL_DEBUG_ASSERT(out.size() <= chars.size());
            chars.extract(out);
        });
        return true;
    }

    /// @brief Returns a string view containing what is currently in the buffer.
    /// This view is invalidated by any operation which changes buffer contents.
    [[nodiscard]]
    constexpr std::u8string_view str() const noexcept
    {
        return as_u8string_view(this->span());
    }
};

} // namespace cowel

#endif

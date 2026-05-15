#ifndef COWEL_POLICY_CAPTURE_HPP
#define COWEL_POLICY_CAPTURE_HPP

#include <memory_resource>
#include <utility>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/char_sequence_ops.hpp"

#include "cowel/policy/content_policy.hpp"

#include "cowel/output_language.hpp"

namespace cowel {

/// @brief Content policy which redirects the output of another content policy
/// into a `std::pmr::vector`.
struct Capturing_Ref_Text_Sink : virtual Text_Sink {
private:
    std::pmr::vector<char8_t>& m_out;
    Output_Language m_language;

public:
    [[nodiscard]]
    explicit Capturing_Ref_Text_Sink(
        std::pmr::vector<char8_t>& out,
        const Output_Language language,
        const Text_Sink_Flags flags = Text_Sink_Flags::none
    ) noexcept
        : Text_Sink { flags }
        , m_out { out }
        , m_language { language }
    {
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>& operator*() &
    {
        return m_out;
    }

    void write(Char_Sequence8 chars, const Output_Language lang) override
    {
        COWEL_DEBUG_ASSERT(lang == m_language);
        if constexpr (enable_empty_string_assertions) {
            COWEL_ASSERT(!chars.empty());
        }
        append(m_out, chars);
    }
};

/// @brief Content policy which redirects the output of another content policy
/// into a `std::pmr::vector`.
struct Vector_Text_Sink : virtual Text_Sink {
private:
    std::pmr::vector<char8_t> m_out;
    Output_Language m_language;

public:
    [[nodiscard]]
    explicit Vector_Text_Sink(
        const Output_Language language,
        std::pmr::memory_resource* const memory,
        const Text_Sink_Flags flags = Text_Sink_Flags::none
    )
        : Text_Sink { flags }
        , m_out { memory }
        , m_language { language }
    {
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>& operator*() &
    {
        return m_out;
    }

    [[nodiscard]]
    const std::pmr::vector<char8_t>& operator*() const&
    {
        return m_out;
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>&& operator*() &&
    {
        return std::move(m_out);
    }

    [[nodiscard]]
    const std::pmr::vector<char8_t>&& operator*() const&&
    {
        return std::move(m_out);
    }

    [[nodiscard]]
    std::pmr::vector<char8_t>* operator->()
    {
        return &m_out;
    }
    [[nodiscard]]
    const std::pmr::vector<char8_t>* operator->() const
    {
        return &m_out;
    }

    void write(Char_Sequence8 chars, const Output_Language lang) override
    {
        COWEL_DEBUG_ASSERT(lang == m_language);
        if constexpr (enable_empty_string_assertions) {
            COWEL_ASSERT(!chars.empty());
        }
        append(m_out, chars);
    }
};

} // namespace cowel

#endif

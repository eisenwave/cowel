#ifndef MMML_SIMPLE_BIBLIOGRAPHY_HPP
#define MMML_SIMPLE_BIBLIOGRAPHY_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "mmml/util/transparent_comparison.hpp"

#include "mmml/services.hpp"

namespace mmml {

/// @brief A bibliography implementation
struct Simple_Bibliography : Bibliography {
protected:
    std::optional<std::unordered_map<
        std::pmr::u8string,
        Stored_Document_Info,
        Basic_Transparent_String_View_Hash<char8_t>,
        Basic_Transparent_String_View_Equals<char8_t>>>
        m_map;

public:
    Simple_Bibliography() = default;

    [[nodiscard]]
    const Document_Info* find(std::u8string_view id) override
    {
        if (!m_map) {
            return nullptr;
        }
        const auto it = m_map->find(id);
        return it == m_map->end() ? nullptr : &it->second.info;
    }

    bool insert(std::pmr::u8string&& id, Stored_Document_Info&& info) override
    {
        MMML_ASSERT(id == info.info.id);
        if (!m_map) {
            m_map.emplace();
            const auto [_, success] = m_map->emplace(std::move(id), std::move(info));
            MMML_ASSERT(success);
            return true;
        }
        const auto [_, success] = m_map->try_emplace(std::move(id), std::move(info));
        return success;
    }
};

inline constinit Simple_Bibliography simple_bibliography;

} // namespace mmml

#endif

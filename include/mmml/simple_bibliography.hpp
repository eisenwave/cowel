#ifndef MMML_SIMPLE_BIBLIOGRAPHY_HPP
#define MMML_SIMPLE_BIBLIOGRAPHY_HPP

#include <optional>
#include <string_view>
#include <unordered_set>

#include "mmml/services.hpp"

namespace mmml {

/// @brief A bibliography implementation
struct Simple_Bibliography : Bibliography {
protected:
    struct Hash {
        using is_transparent = void;

        [[nodiscard]]
        std::size_t operator()(std::u8string_view str) const
        {
            return std::hash<std::u8string_view> {}(str);
        }

        [[nodiscard]]
        std::size_t operator()(const Stored_Document_Info& info) const
        {
            return operator()(info.info.id);
        }
    };

    struct Equals {
        using is_transparent = void;

        template <typename T, typename U>
        [[nodiscard]]
        bool operator()(const T& x, const U& y) const
        {
            return as_string(x) == as_string(y);
        }

        [[nodiscard]]
        static std::u8string_view as_string(std::u8string_view str)
        {
            return str;
        }
        [[nodiscard]]
        static std::u8string_view as_string(const Stored_Document_Info& info)
        {
            return info.info.id;
        }
    };

    std::optional<std::unordered_set<Stored_Document_Info, Hash, Equals>> m_map;

public:
    Simple_Bibliography() = default;

    [[nodiscard]]
    const Document_Info* find(std::u8string_view id) const override
    {
        if (!m_map) {
            return nullptr;
        }
        const auto it = m_map->find(id);
        return it == m_map->end() ? nullptr : &it->info;
    }

    bool insert(Stored_Document_Info&& info) override
    {
        if (!m_map) {
            m_map.emplace();
        }
        const auto [_, success] = m_map->insert(std::move(info));
        return success;
    }

    void clear() override
    {
        if (m_map) {
            m_map->clear();
        }
    }
};

inline constinit Simple_Bibliography simple_bibliography;

} // namespace mmml

#endif

#ifndef MMML_DOCUMENT_WRITER_HPP
#define MMML_DOCUMENT_WRITER_HPP

#include <map>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include "mmml/util/assert.hpp"
#include "mmml/util/html_writer.hpp"
#include "mmml/util/transparent_comparison.hpp"
#include "mmml/util/unicode.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

namespace detail {

using Suppress_Unused_Include_Transparent_Less = Basic_Transparent_String_View_Less<void>;

} // namespace detail

struct Document_Sections {
public:
    // The choice of std::map over std::unordered_map is deliberate:
    // We require iterator and reference stability in some cases,
    // and std::unordered_map can invalidate iterators and references on rehashing.
    using map_type = std::pmr::
        map<std::pmr::u8string, std::pmr::vector<char8_t>, Transparent_String_View_Less8>;
    using entry_type = map_type::value_type;

    struct [[nodiscard]] Scoped_Section {
    private:
        friend Document_Sections;

        Document_Sections& self;
        entry_type& old;

        Scoped_Section(Document_Sections& self, entry_type& old)
            : self { self }
            , old { old }
        {
        }

    public:
        Scoped_Section(const Scoped_Section&) = delete;
        Scoped_Section& operator=(const Scoped_Section&) = delete;

        ~Scoped_Section()
        {
            self.m_current = &old;
        }
    };

private:
    map_type m_sections;
    entry_type* m_current = &*m_sections.emplace().first;

public:
    [[nodiscard]]
    explicit Document_Sections(std::pmr::memory_resource* memory)
        : m_sections { memory }
    {
    }

    /// @brief Returns the memory resource that this object was constructed with.
    [[nodiscard]]
    std::pmr::memory_resource* get_memory() const
    {
        return m_sections.get_allocator().resource();
    }

    /// @brief Returns a pointer to the section named `section` if one exists;
    /// otherwise returns null.
    ///
    /// No allocations are performed.
    [[nodiscard]]
    entry_type* find(std::u8string_view section)
    {
        const auto existing_iter = m_sections.find(section);
        return existing_iter != m_sections.end() ? &*existing_iter : nullptr;
    }

    /// @brief Returns a pointer to the section named `section` if one exists;
    /// otherwise returns null.
    ///
    /// No allocations are performed.
    [[nodiscard]]
    const entry_type* find(std::u8string_view section) const
    {
        const auto existing_iter = m_sections.find(section);
        return existing_iter != m_sections.end() ? &*existing_iter : nullptr;
    }

    /// @brief Creates a new section named `section` if one doesn't exist yet.
    /// Returns a reference to the new or existing one.
    ///
    /// Allocates a new key string if the section doesn't exist yet.
    entry_type& make(std::u8string_view section)
    {
        if (entry_type* const existing = find(section)) {
            return *existing;
        }
        const auto [iter, success] = m_sections.emplace(
            std::pmr::u8string { section, get_memory() }, std::pmr::vector<char8_t> { get_memory() }
        );
        MMML_ASSERT(success);
        return *iter;
    }

    /// @brief Like the overload taking `std::u8string_view`,
    /// but if the section does not yet exist,
    /// doesn't allocate a new string for the key name.
    entry_type& make(std::pmr::u8string&& section)
    {
        if (entry_type* const existing = find(section)) {
            return *existing;
        }
        const auto [iter, success]
            = m_sections.emplace(std::move(section), std::pmr::vector<char8_t> { get_memory() });
        MMML_ASSERT(success);
        return *iter;
    }

    /// @brief Sets the current section to the given `section` if one already exists,
    /// and returns a pointer to the section entry;
    /// otherwise returns null.
    ///
    /// No allocations are performed.
    [[nodiscard]]
    entry_type* try_go_to(std::u8string_view section)
    {
        const auto existing_iter = m_sections.find(section);
        entry_type* result = existing_iter != m_sections.end() ? &*existing_iter : nullptr;
        m_current = result;
        return result;
    }

    /// @brief Sets the current section to an existing one or a newly created one named `section`,
    /// and returns a reference to that section.
    ///
    /// Allocates a new key string if a section doesn't exist yet.
    entry_type& go_to(std::u8string_view section)
    {
        entry_type& result = make(section);
        m_current = &result;
        return result;
    }

    /// @brief Like the overload taking `std::u8string_view`,
    /// but if the section does not yet exist,
    /// doesn't allocate a new string for the key name.
    entry_type& go_to(std::pmr::u8string&& section)
    {
        entry_type& result = make(std::move(section));
        m_current = &result;
        return result;
    }

    /// @brief Calls `go_to(section)` and returns a `Scoped_Section` which,
    /// upon destruction, sets the current section to the section prior to `go_to`.
    ///
    /// This is useful for temporarily writing content to a different section.
    Scoped_Section go_to_scoped(std::u8string_view section)
    {
        entry_type* const old = m_current;
        go_to(section);
        return Scoped_Section { *this, *old };
    }

    /// @brief Returns a reference to the current section.
    [[nodiscard]]
    entry_type& current() noexcept
    {
        return *m_current;
    }

    /// @brief Returns a reference to the current section.
    [[nodiscard]]
    const entry_type& current() const noexcept
    {
        return *m_current;
    }

    [[nodiscard]]
    std::u8string_view current_name() const noexcept
    {
        return current().first;
    }

    /// @brief Returns the output characters of the current section.
    [[nodiscard]]
    std::pmr::vector<char8_t>& current_text() noexcept
    {
        return current().second;
    }

    /// @brief Returns the output characters of the current section.
    [[nodiscard]]
    const std::pmr::vector<char8_t>& current_text() const noexcept
    {
        return current().second;
    }

    /// @brief Equivalent to `HTML_Writer { current_text() }`.
    [[nodiscard]]
    HTML_Writer current_html() noexcept
    {
        return HTML_Writer { current_text() };
    }
};

/// @brief Appends a "section reference" to `out`.
/// This works by mapping the length onto a code point within the
/// Supplementary Private Use Area-A block,
/// and encoding that as UTF-8.
/// The given name is then appended as is.
/// @returns `name.size() <= 65636`.
/// A name beyond that size cannot be encoded as a section reference.
inline bool reference_section(std::pmr::vector<char8_t>& out, std::u8string_view name)
{
    constexpr std::size_t max_length = supplementary_pua_a_min - supplementary_pua_a_max;
    if (name.size() > max_length) {
        return false;
    }

    const utf8::Code_Units_And_Length units_and_length
        = utf8::encode8_unchecked(supplementary_pua_a_min + char32_t(name.size()));
    out.insert(out.end(), units_and_length.begin(), units_and_length.end());
    return true;
}

inline bool reference_section(HTML_Writer& out, std::u8string_view name)
{
    return reference_section(out.get_output(), name);
}

} // namespace mmml

#endif

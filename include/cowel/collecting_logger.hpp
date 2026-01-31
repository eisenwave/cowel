#ifndef COWEL_COLLECTING_LOGGER_HPP
#define COWEL_COLLECTING_LOGGER_HPP

#include <algorithm>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence_ops.hpp"
#include "cowel/util/severity.hpp"
#include "cowel/util/strings.hpp"

#include "cowel/diagnostic.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Collected_Diagnostic {
    Severity severity;
    std::pmr::u8string id;
    File_Source_Span location;
    std::pmr::u8string message;

    [[nodiscard]]
    Collected_Diagnostic(const Diagnostic& d, std::pmr::memory_resource* const memory)
        : severity { d.severity }
        , id { to_string(d.id, memory) }
        , location { d.location }
        , message { to_string(d.message, memory) }
    {
    }
};

struct Collecting_Logger final : Logger {
    std::pmr::vector<Collected_Diagnostic> diagnostics;

    [[nodiscard]]
    explicit Collecting_Logger(std::pmr::memory_resource* const memory)
        : Logger { Severity::min }
        , diagnostics { memory }
    {
    }

    void operator()(const Diagnostic diagnostic) final
    {
        std::pmr::memory_resource* const memory = diagnostics.get_allocator().resource();
        diagnostics.emplace_back(diagnostic, memory);
    }

    [[nodiscard]]
    bool nothing_logged() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]]
    bool was_logged(const std::u8string_view id) const
    {
        return std::ranges::find(diagnostics, id, &Collected_Diagnostic::id) != diagnostics.end();
    }
};

struct Expecting_Logger final : Logger {
private:
    const Severity m_expected_severity;
    const std::u8string_view m_expected_id;
    bool m_expected_logged = false;
    std::pmr::vector<Collected_Diagnostic> m_violations;

public:
    explicit Expecting_Logger(
        Severity min_severity,
        Severity expected_severity,
        std::u8string_view expected_id,
        std::pmr::memory_resource* memory
    )
        : Logger { min_severity }
        , m_expected_severity { expected_severity }
        , m_expected_id { expected_id }
        , m_violations { memory }
    {
    }

    [[nodiscard]]
    std::span<const Collected_Diagnostic> get_violations() const
    {
        return m_violations;
    }

    void operator()(const Diagnostic diagnostic) override
    {
        std::pmr::vector<char8_t> id_data;
        append(id_data, diagnostic.id);
        const auto id_string = as_u8string_view(id_data);
        if (diagnostic.severity == m_expected_severity && id_string == m_expected_id) {
            m_expected_logged = true;
            return;
        }
        // Additional warnings or errors are not considered a violation,
        // but getting anything with greater severity should not happen.
        if (diagnostic.severity > m_expected_severity) {
            static_assert(std::is_trivially_copyable_v<Diagnostic>);
            std::pmr::memory_resource* const memory = m_violations.get_allocator().resource();
            m_violations.emplace_back(diagnostic, memory);
        }
    }

    [[nodiscard]]
    bool was_expected_logged() const
    {
        return m_expected_logged;
    }
};

} // namespace cowel

#endif

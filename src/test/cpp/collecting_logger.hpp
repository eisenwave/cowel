#ifndef COWEL_COLLECTING_LOGGER_HPP
#define COWEL_COLLECTING_LOGGER_HPP

#include <memory_resource>
#include <string_view>
#include <vector>

#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Collected_Diagnostic {
    Severity severity;
    std::u8string_view id;
};

struct Collecting_Logger final : Logger {
    std::pmr::vector<Collected_Diagnostic> diagnostics;

    [[nodiscard]]
    Collecting_Logger(std::pmr::memory_resource* memory)
        : Logger { Severity::min }
        , diagnostics { memory }
    {
    }

    void operator()(const Diagnostic& diagnostic) final
    {
        diagnostics.push_back({ diagnostic.severity, diagnostic.id });
    }

    [[nodiscard]]
    bool nothing_logged() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]]
    bool was_logged(std::u8string_view id) const
    {
        return std::ranges::find(diagnostics, id, &Collected_Diagnostic::id) != diagnostics.end();
    }
};

} // namespace cowel

#endif

#ifndef COWEL_COLLECTING_LOGGER_HPP
#define COWEL_COLLECTING_LOGGER_HPP

#include <memory_resource>
#include <string_view>
#include <utility>
#include <vector>

#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Collecting_Logger final : Logger {
    std::pmr::vector<Diagnostic> diagnostics;

    [[nodiscard]]
    Collecting_Logger(std::pmr::memory_resource* memory)
        : Logger { Severity::min }
        , diagnostics { memory }
    {
    }

    void operator()(Diagnostic&& diagnostic) final
    {
        diagnostics.push_back(std::move(diagnostic));
    }

    [[nodiscard]]
    bool nothing_logged() const
    {
        return diagnostics.empty();
    }

    [[nodiscard]]
    bool was_logged(std::u8string_view id) const
    {
        return std::ranges::find(diagnostics, id, &Diagnostic::id) != diagnostics.end();
    }
};

} // namespace cowel

#endif

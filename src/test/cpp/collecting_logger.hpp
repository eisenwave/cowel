#ifndef MMML_COLLECTING_LOGGER_HPP
#define MMML_COLLECTING_LOGGER_HPP

#include <memory_resource>
#include <string_view>
#include <utility>
#include <vector>

#include "mmml/diagnostic.hpp"
#include "mmml/services.hpp"

#include "mmml/fwd.hpp"

namespace mmml {

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

} // namespace mmml

#endif

#ifndef COWEL_COLLECTING_LOGGER_HPP
#define COWEL_COLLECTING_LOGGER_HPP

#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include "cowel/util/char_sequence_ops.hpp"

#include "cowel/diagnostic.hpp"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Collected_Diagnostic {
    Severity severity;
    std::pmr::u8string id;
    File_Source_Span location;
    std::pmr::u8string message;

    [[nodiscard]]
    Collected_Diagnostic(const Diagnostic& d, std::pmr::memory_resource* memory)
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
    Collecting_Logger(std::pmr::memory_resource* memory)
        : Logger { Severity::min }
        , diagnostics { memory }
    {
    }

    void operator()(Diagnostic diagnostic) final
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
    bool was_logged(std::u8string_view id) const
    {
        return std::ranges::find(diagnostics, id, &Collected_Diagnostic::id) != diagnostics.end();
    }
};

} // namespace cowel

#endif

#ifndef COWEL_SEVERITY_HPP
#define COWEL_SEVERITY_HPP

#include <string_view>

#include "cowel/cowel.h"
#include "cowel/fwd.hpp"

namespace cowel {

enum struct Severity : Default_Underlying {
    min = COWEL_SEVERITY_MIN,
    trace = COWEL_SEVERITY_TRACE,
    debug = COWEL_SEVERITY_DEBUG,
    info = COWEL_SEVERITY_INFO,
    soft_warning = COWEL_SEVERITY_SOFT_WARNING,
    warning = COWEL_SEVERITY_WARNING,
    error = COWEL_SEVERITY_ERROR,
    fatal = COWEL_SEVERITY_FATAL,
    max = COWEL_SEVERITY_MAX,
    none = COWEL_SEVERITY_NONE,
};

[[nodiscard]]
constexpr std::u8string_view severity_tag(Severity severity)
{
    using enum Severity;
    switch (severity) {
    case min: return u8"MIN";
    case trace: return u8"TRACE";
    case debug: return u8"DEBUG";
    case info: return u8"INFO";
    case soft_warning: return u8"SOFTWARN";
    case warning: return u8"WARNING";
    case error: return u8"ERROR";
    case fatal: return u8"FATAL";
    case none: break;
    }
    return u8"???";
}

} // namespace cowel

#endif

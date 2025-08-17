#ifndef COWEL_SEVERITY_HPP
#define COWEL_SEVERITY_HPP

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

} // namespace cowel

#endif

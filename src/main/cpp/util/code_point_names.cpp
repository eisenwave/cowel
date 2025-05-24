#include <string_view>

#include "ulight/impl/platform.h"

ULIGHT_DIAGNOSTIC_PUSH()
ULIGHT_DIAGNOSTIC_IGNORED("-Wsign-conversion")
ULIGHT_DIAGNOSTIC_IGNORED("-Wshorten-64-to-32")
#include "cowel/cedilla/name_to_cp.hpp"
ULIGHT_DIAGNOSTIC_POP()

#include "cowel/util/code_point_names.hpp"
#include "cowel/util/strings.hpp"

namespace cowel {

char32_t code_point_by_name(std::u8string_view name) noexcept
{
    const char32_t result = uni::cp_from_name(as_string_view(name));
    return result > 0x10'FFFF ? char32_t(-1) : result;
}

} // namespace cowel

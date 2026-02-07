#include <cstddef>
#include <string_view>

#include "cowel/util/strings.hpp"

#include "cowel/regexp.hpp"

#ifndef COWEL_BUILD_WASM
#error "This file should only compile on WASM builds."
#endif

extern "C" {

/// @brief Compiles the pattern on the host and returns a handle.
/// If the pattern is not valid, returns a value-initialized handle.
/// @param pattern A pointer to the UTF-8-encoded pattern with ECMAScript flavor.
/// @param length The length of the pattern, in code points.
COWEL_WASM_IMPORT("env", "reg_exp_compile")
cowel::Reg_Exp_Handle cowel_reg_exp_compile(const char* pattern, std::size_t length);

/// @brief Deletes a host regular expression with the given handle,
/// if that handle refers to a valid regular expression.
/// @return `true` iff the given handle was valid.
COWEL_WASM_IMPORT("env", "reg_exp_delete")
bool cowel_reg_exp_delete(cowel::Reg_Exp_Handle r);

/// @brief Returns `true` if the given regular expression `r` matches the given
/// `string` in its entirety.
/// @param r The handle.
/// @param string A pointer to the searched UTF-8 string.
/// @param length The length of the searched string, in code units.
COWEL_WASM_IMPORT("env", "reg_exp_match")
cowel::Reg_Exp_Status
cowel_reg_exp_match(cowel::Reg_Exp_Handle r, const char* string, std::size_t length);

struct cowel_reg_exp_search_result {
    std::size_t index;
    std::size_t length;
};

/// @brief Returns `true` if the given regular expression `r` is found in the given `string`.
/// @param search_result A pointer to where the search results should be written.
/// @param r The handle.
/// @param string A pointer to the searched UTF-8 string.
/// @param length The length of the searched string, in code units.
COWEL_WASM_IMPORT("env", "reg_exp_search")
cowel::Reg_Exp_Status cowel_reg_exp_search(
    cowel::Reg_Exp_Match* search_result,
    cowel::Reg_Exp_Handle r,
    const char* string,
    std::size_t length
);

//
}

namespace cowel {

Unique_Host_Reg_Exp::~Unique_Host_Reg_Exp()
{
    const bool success = cowel_reg_exp_delete(m_handle);
    COWEL_ASSERT(success);
    m_handle = {};
}

[[nodiscard]]
Result<Reg_Exp, Reg_Exp_Error_Code> Reg_Exp::make(const std::u8string_view pattern)
{
    const auto pattern_sv = as_string_view(pattern);
    const Reg_Exp_Handle handle = cowel_reg_exp_compile(pattern_sv.data(), pattern_sv.size());
    if (handle == Reg_Exp_Handle {}) {
        return Reg_Exp_Error_Code::bad_pattern;
    }
    return Reg_Exp { gc_ref_make<Unique_Host_Reg_Exp>(handle) };
}

[[nodiscard]]
Reg_Exp_Status Reg_Exp::test(const std::u8string_view string) const
{
    const Reg_Exp_Handle handle = m_ref->handle();
    COWEL_ASSERT(handle != Reg_Exp_Handle {});
    const auto string_sv = as_string_view(string);

    const auto status = cowel_reg_exp_match(handle, string_sv.data(), string_sv.length());
    return Reg_Exp_Status(status);
}

[[nodiscard]]
Reg_Exp_Search_Result Reg_Exp::search(const std::u8string_view string) const
{
    const Reg_Exp_Handle handle = m_ref->handle();
    COWEL_ASSERT(handle != Reg_Exp_Handle {});
    const auto string_sv = as_string_view(string);

    Reg_Exp_Search_Result result;
    const auto status
        = cowel_reg_exp_search(&result.match, handle, string_sv.data(), string_sv.length());
    result.status = Reg_Exp_Status(status);
    return result;
}

} // namespace cowel

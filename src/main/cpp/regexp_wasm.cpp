#include <cstddef>
#include <string_view>

#include "cowel/cowel_lib.hpp"
#include "cowel/regexp.hpp"

#ifndef COWEL_BUILD_WASM
#error "This file should only compile on WASM builds."
#endif

extern "C" {

/// @brief Compiles the pattern on the host and returns a handle.
/// If the pattern is not valid, returns a value-initialized handle.
/// @param pattern A pointer to the UTF-8-encoded pattern with ECMAScript flavor.
/// @param length The length of the pattern, in code points.
/// @param flags The set of flags to construct the regular expression with.
COWEL_WASM_IMPORT("env", "reg_exp_compile")
cowel::Reg_Exp_Handle
cowel_reg_exp_compile(const char8_t* pattern, std::size_t length, cowel::Reg_Exp_Flags flags);

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
cowel_reg_exp_match(cowel::Reg_Exp_Handle r, const char8_t* string, std::size_t length);

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
    const char8_t* string,
    std::size_t length
);

/// @brief Replaces every match of `r` within the given string with
/// @param r The handle.
/// @param string A pointer to the searched UTF-8 string.
/// @param length The length of the searched string, in code units.
COWEL_WASM_IMPORT("env", "reg_exp_replace_all")
cowel::Reg_Exp_Status cowel_reg_exp_replace_all(
    cowel_mutable_string_view_u8* result,
    cowel::Reg_Exp_Handle r,
    const char8_t* string,
    std::size_t string_length,
    const char8_t* replacement,
    std::size_t replacement_length
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
Result<Reg_Exp, Reg_Exp_Error_Code> Reg_Exp::make(
    const std::u8string_view pattern, //
    const Reg_Exp_Flags flags
)
{
    const Reg_Exp_Handle handle = cowel_reg_exp_compile(pattern.data(), pattern.size(), flags);
    if (handle == Reg_Exp_Handle {}) {
        return Reg_Exp_Error_Code::bad_pattern;
    }
    return Reg_Exp { gc_ref_make<Unique_Host_Reg_Exp>(handle) };
}

Reg_Exp_Status Reg_Exp::match(const std::u8string_view string) const
{
    const Reg_Exp_Handle handle = m_ref->handle();
    COWEL_ASSERT(handle != Reg_Exp_Handle {});

    return cowel_reg_exp_match(handle, string.data(), string.length());
}

Reg_Exp_Search_Result Reg_Exp::search(const std::u8string_view string) const
{
    const Reg_Exp_Handle handle = m_ref->handle();
    COWEL_ASSERT(handle != Reg_Exp_Handle {});

    Reg_Exp_Search_Result result;
    result.status = cowel_reg_exp_search(&result.match, handle, string.data(), string.length());
    return result;
}

Reg_Exp_Status Reg_Exp::replace_all(
    std::pmr::vector<char8_t>& out,
    const std::u8string_view string,
    const std::u8string_view replacement
) const
{
    const Reg_Exp_Handle handle = m_ref->handle();
    COWEL_ASSERT(handle != Reg_Exp_Handle {});

    cowel_mutable_string_view_u8 replaced {};
    const auto status = cowel_reg_exp_replace_all(
        &replaced, handle, string.data(), string.size(), replacement.data(), replacement.size()
    );
    if (status != Reg_Exp_Status::matched) {
        COWEL_ASSERT(replaced.text == nullptr);
        return status;
    }
    if (replaced.text) {
        const auto result = as_u8string_view(replaced);
        out.insert(out.end(), result.data(), result.data() + result.size());
        // This assumes that no custom allocators have been provided,
        // so the allocation must have taken place using cowel_alloc.
        // See also wasm.cpp.
        cowel_free(replaced.text, replaced.length, alignof(char8_t));
    }
    return status;
}

} // namespace cowel

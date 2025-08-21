#ifndef COWEL_RELATIVE_FILE_LOADER_HPP
#define COWEL_RELATIVE_FILE_LOADER_HPP

#include <filesystem>
#include <memory_resource>
#include <vector>

#include "cowel/util/char_sequence.hpp"
#include "cowel/util/io.hpp"

#include "cowel/cowel.h"
#include "cowel/fwd.hpp"
#include "cowel/services.hpp"

namespace cowel {

struct Owned_File_Entry {
    std::filesystem::path path;
    std::u8string path_string;
    std::pmr::vector<char8_t> text;
};

/// @brief A `File_Loader` implementation which can be used both as
/// an internal implementation of `File_Loader` for testing
/// and as an external implementation which is fed into the `cowel.h` top-level API.
///
/// This class loads files relative to a given constant base directory.
struct Relative_File_Loader final : File_Loader {
private:
    std::filesystem::path m_base;
    std::pmr::vector<Owned_File_Entry> m_entries;

public:
    [[nodiscard]]
    explicit Relative_File_Loader(std::filesystem::path&& base, std::pmr::memory_resource* memory);

    struct Complete_Result {
        cowel_file_result_u8 file_result;
        Owned_File_Entry& entry;
    };

    [[nodiscard]]
    const Owned_File_Entry& at(File_Id id) const
    {
        const auto value = int(id);
        // This could fail if we accidentally call `at(File_Id::main)`,
        // given that the main document is considered to simply exist in the environment,
        // rather than being loaded here and having an entry.
        COWEL_ASSERT(value >= 0);
        return m_entries[std::size_t(value)];
    }

    /// @brief External implementation to be used with `cowel.h` API.
    [[nodiscard]]
    Complete_Result do_load(Char_Sequence8 path_chars, File_Id relative_to);

    /// @brief Internal implementation for `File_Loader` service.
    [[nodiscard]]
    Result<File_Entry, File_Load_Error> load(Char_Sequence8 path, File_Id relative_to) final;
};

} // namespace cowel

#endif

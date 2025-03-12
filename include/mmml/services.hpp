#ifndef MMML_SERVICES_HPP
#define MMML_SERVICES_HPP

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "mmml/util/annotation_span.hpp"
#include "mmml/util/assert.hpp"
#include "mmml/util/result.hpp"

#include "mmml/diagnostic.hpp"
#include "mmml/fwd.hpp"

namespace mmml {

namespace detail {
using Suppress_Unused_Include_Annotation_Span = Annotation_Span<void>;
} // namespace detail

enum struct Syntax_Highlight_Error : Default_Underlying {
    /// @brief A given language hint is not supported,
    /// and the language couldn't be determined automatically.
    unsupported_language,
    /// @brief Code cannot be highlighted because it's ill-formed,
    /// and the syntax highlighter does not tolerate ill-formed code.
    bad_code,
};

struct Syntax_Highlighter {

    /// @brief Returns a set of supported languages in no particular order.
    /// These languages can be used in `operator()` as hints.
    [[nodiscard]]
    virtual std::span<const std::u8string_view> get_supported_languages() const
        = 0;

    /// @brief Applies syntax highlighting to the given `code`.
    /// Spans of highlighted source code are appended to `out`.
    /// If a failed result is returned,
    /// nothing is appended to `out`.
    /// @param out Where the spans are appended to.
    /// @param code The source code.
    /// @param language A language hint (possibly an empty string).
    /// This should be one of the languages returned by `get_supported_languages`.
    [[nodiscard]]
    virtual Result<void, Syntax_Highlight_Error> operator()(
        std::pmr::vector<HLJS_Annotation_Span>& out,
        std::u8string_view code,
        std::u8string_view language = {}
    ) const
        = 0;
};

/// @brief A `Syntax_Highlighter` that supports no languages.
struct No_Support_Syntax_Highlighter final : Syntax_Highlighter {

    [[nodiscard]]
    std::span<const std::u8string_view> get_supported_languages() const final
    {
        return {};
    }

    [[nodiscard]]
    Result<void, Syntax_Highlight_Error>
    operator()(std::pmr::vector<HLJS_Annotation_Span>&, std::u8string_view, std::u8string_view = {})
        const final
    {
        return Syntax_Highlight_Error::unsupported_language;
    }
};

inline constexpr No_Support_Syntax_Highlighter no_support_syntax_highlighter;

struct Author_Info {
    /// @brief Full name. For example, `Donald Knuth`.
    std::u8string_view name;
    /// @brief E-mail address. For example, `knuth@gmail.com`.
    std::u8string_view email;
    /// @brief Affiliation, such as a company. For example, `Microsoft`.
    std::u8string_view affiliation;
};

struct Document_Info {
    /// @brief ID by which the document is referenced elsewhere. For example, `Knuth01`.
    std::u8string_view id;
    /// @brief Title of the publication.
    std::u8string_view title;
    /// @brief The date of publication.
    std::u8string_view date;
    /// @brief The publisher.
    std::u8string_view publisher;
    /// @brief The primary (short) link to the document.
    std::u8string_view link;
    /// @brief The long link to the document.
    std::u8string_view long_link;
    /// @brief A link to issue tracking for the document.
    /// For example, a GitHub issue URL for WG21 papers.
    std::u8string_view issue_link;
    /// @brief A list of authors.
    std::span<const Author_Info> authors;
};

struct Stored_Document_Info {
    std::pmr::vector<std::byte> storage;
    Document_Info info;
};

struct Document_Finder {
    [[nodiscard]]
    virtual std::optional<Stored_Document_Info> operator()(std::u8string_view id) const
        = 0;
};

struct No_Support_Document_Finder final : Document_Finder {
    [[nodiscard]]
    std::optional<Stored_Document_Info> operator()(std::u8string_view) const final
    {
        return {};
    }
};

inline constexpr No_Support_Document_Finder no_support_document_finder;

struct Logger {
private:
    Severity m_min_severity;

public:
    [[nodiscard]]
    constexpr explicit Logger(Severity min_severity)
    {
        set_min_severity(min_severity);
    }

    [[nodiscard]]
    constexpr Severity get_min_severity() const
    {
        return m_min_severity;
    }

    constexpr void set_min_severity(Severity severity)
    {
        MMML_ASSERT(severity <= Severity::none);
        m_min_severity = severity;
    }

    [[nodiscard]]
    constexpr bool can_log(Severity severity) const
    {
        return severity >= m_min_severity;
    }

    constexpr virtual void operator()(Diagnostic&& diagnostic) const = 0;
};

struct Ignorant_Logger final : Logger {
    using Logger::Logger;

    void operator()(Diagnostic&&) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
        const final
    {
    }
};

inline constexpr Ignorant_Logger ignorant_logger { Severity::none };

} // namespace mmml

#endif

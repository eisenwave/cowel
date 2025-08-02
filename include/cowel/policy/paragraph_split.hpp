#ifndef COWEL_POLICY_PARAGRAPH_SPLIT_HPP
#define COWEL_POLICY_PARAGRAPH_SPLIT_HPP

#include <cstddef>
#include <memory_resource>
#include <string_view>

#include "cowel/util/assert.hpp"
#include "cowel/util/char_sequence.hpp"

#include "cowel/policy/content_policy.hpp"
#include "cowel/policy/html.hpp"

#include "cowel/ast.hpp"
#include "cowel/context.hpp"
#include "cowel/directive_processing.hpp"
#include "cowel/output_language.hpp"
#include "cowel/parse_utils.hpp"
#include "cowel/util/char_sequence_ops.hpp"

namespace cowel {

struct Paragraph_Split_Policy final : virtual HTML_Content_Policy {
private:
    struct Directive_Depth_Guard;

    static constexpr std::u8string_view opening_tag = u8"<p>";
    static constexpr std::u8string_view closing_tag = u8"</p>";

    Paragraphs_State m_state;
    Blank_Line_Initial_State m_line_state = Blank_Line_Initial_State::middle;
    std::pmr::memory_resource* m_memory;

    // The following two members have vaguely similar purposes,
    // but they need to be distinct
    // because when the current guard is released, the depth goes down,
    // but the guard remains in place until it actually goes out of scope.
    // This makes it possible to safely call
    // `activate_paragraphs_in_directive()` multiple times in a row.

    std::size_t m_directive_depth = 0;
    Directive_Depth_Guard* m_current_guard = nullptr;

    struct Directive_Depth_Guard {
    private:
        Paragraph_Split_Policy& m_self;
        Directive_Depth_Guard* m_parent_guard;
        bool released = false;

    public:
        [[nodiscard]]
        explicit Directive_Depth_Guard(Paragraph_Split_Policy& self) noexcept
            : m_self { self }
            , m_parent_guard { self.m_current_guard }
        {
            ++m_self.m_directive_depth;
            m_self.m_current_guard = this;
        }

        Directive_Depth_Guard(const Directive_Depth_Guard&) = delete;
        Directive_Depth_Guard& operator=(const Directive_Depth_Guard&) = delete;

        ~Directive_Depth_Guard() noexcept(false)
        {
            release();
            m_self.m_current_guard = m_parent_guard;
        }

        void release()
        {
            if (!released) {
                released = true;
                COWEL_ASSERT(m_self.m_directive_depth != 0);
                --m_self.m_directive_depth;
            }
        }
    };

public:
    [[nodiscard]]
    explicit Paragraph_Split_Policy(
        Text_Sink& parent,
        std::pmr::memory_resource* memory,
        Paragraphs_State initial_state = Paragraphs_State::outside
    )
        : Text_Sink { Output_Language::html }
        , Content_Policy { Output_Language::html }
        , HTML_Content_Policy { parent }
        , m_state { initial_state }
        , m_memory { memory }
    {
    }

    bool write(Char_Sequence8 chars, Output_Language language) override
    {
        if (m_directive_depth != 0 || language != Output_Language::text) {
            return HTML_Content_Policy::write(chars, language);
        }
        if (chars.empty()) {
            return true;
        }
        const std::u8string_view sv = chars.as_string_view();
        split_into_paragraphs(sv.empty() ? to_string(chars, m_memory) : sv);
        return true;
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Text& t, Context&) override
    {
        if (m_directive_depth != 0) {
            write(t.get_source(), Output_Language::text);
        }
        else {
            split_into_paragraphs(t.get_source());
        }
        return Processing_Status::ok;
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Comment&, Context&) override
    {
        // Comments syntactically include the terminating newline,
        // so a leading newline following a comment would be considered a paragraph break.
        m_line_state = Blank_Line_Initial_State::normal;
        return Processing_Status::ok;
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Escaped& escape, Context&) override
    {
        m_line_state = Blank_Line_Initial_State::middle;
        const std::u8string_view text = expand_escape(escape);
        if (text.empty()) {
            return Processing_Status::ok;
        }
        enter_paragraph();
        HTML_Content_Policy::write(text, Output_Language::text);
        return Processing_Status::ok;
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Directive& directive, Context& context) override
    {
        // The purpose of m_directive_depth is to prevent malformed output which results
        // from directives directly feeding their contents into this policy,
        // interleaved with their own tags.
        //
        // For example, \i{...} should not produce <i><p>...</i> or <i>...</p></i>.
        //
        // Since consume() may be entered recursively for the same policy
        // (e.g. in \paragraphs{\i{\b{...}}}),
        // a simple bool is insufficient to keep track of whether we are in a directive.
        m_line_state = Blank_Line_Initial_State::middle;
        const Directive_Depth_Guard depth_guard { *this };
        return apply_behavior(*this, directive, context);
    }

    [[nodiscard]]
    Processing_Status consume(const ast::Generated& generated, Context&) override
    {
        // We deliberately don't update m_line_state here
        // because paragraph splitting generally operates on syntactical elements.
        write(generated.as_string(), generated.get_type());
        return Processing_Status::ok;
    }

    /// @brief Enables paragraph splitting to take place inside a directive.
    ///
    /// By default, directives are treated as black boxes,
    /// and their contents are not split since this could easily result in corrupted HTML.
    /// However, certain directives like \import rely on paragraph splitting
    /// from the surroundings to apply to any imported content.
    /// Such directives can explicitly opt into paragraph splitting using this member function.
    void inherit_paragraph()
    {
        COWEL_ASSERT(m_current_guard != nullptr);
        m_current_guard->release();
    }

    void enter_paragraph()
    {
        // We check for <= 1 depth rather than zero so that a directive can simply call
        // enter_paragraph() or leave_paragraph() if it appears at the "top level"
        // relative to the paragraph split policy.
        //
        // However, any directives nested within
        if (m_directive_depth <= 1 && m_state == Paragraphs_State::outside) {
            write(opening_tag, Output_Language::html);
            m_state = Paragraphs_State::inside;
        }
    }

    void leave_paragraph()
    {
        if (m_directive_depth <= 1 && m_state == Paragraphs_State::inside) {
            write(closing_tag, Output_Language::html);
            m_state = Paragraphs_State::outside;
        }
    }

    void transition(Paragraphs_State state)
    {
        switch (state) {
        case Paragraphs_State::inside: enter_paragraph(); break;
        case Paragraphs_State::outside: leave_paragraph(); break;
        }
    }

private:
    void split_into_paragraphs(std::u8string_view text);
};

} // namespace cowel

#endif

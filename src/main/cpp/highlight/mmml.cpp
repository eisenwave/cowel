#include <cstddef>
#include <memory_resource>
#include <span>
#include <string_view>
#include <vector>

#include "mmml/util/annotation_span.hpp"

#include "mmml/fwd.hpp"
#include "mmml/parse.hpp"

#include "mmml/highlight/highlight.hpp"
#include "mmml/highlight/mmml.hpp"

namespace mmml {

bool highlight_mmml(
    std::pmr::vector<Annotation_Span<Highlight_Type>>& out,
    std::u8string_view source,
    std::pmr::memory_resource* memory,
    const Highlight_Options& options
)
{
    std::pmr::vector<AST_Instruction> instructions { memory };
    parse(instructions, source);
    highlight_mmml(out, source, instructions, options);
    return true;
}

void highlight_mmml( //
    std::pmr::vector<Annotation_Span<Highlight_Type>>& out,
    std::u8string_view source,
    std::span<const AST_Instruction> instructions,
    const Highlight_Options& options
)
{
    std::size_t index = 0;
    const auto emit = [&](std::size_t length, Highlight_Type type) {
        const bool coalesce = options.coalescing && !out.empty() && out.back().value == type
            && out.back().end() == index;
        if (coalesce) {
            out.back().length += length;
        }
        else {
            out.emplace_back(index, length, type);
        }
        index += length;
    };

    [[maybe_unused]]
    std::size_t in_comment
        = 0;
    for (const auto& i : instructions) {
        switch (i.type) {
            using enum AST_Instruction_Type;
        case skip: //
            index += i.n;
            break;
        case escape: //
            emit(i.n, Highlight_Type::string_escape);
            break;
        case text: //
            index += i.n;
            break;
        case argument_name: //
            emit(i.n, Highlight_Type::attribute);
            break;
        case push_directive: {
            const std::u8string_view directive_name = source.substr(index, i.n);
            // TODO: highlight comment contents specially,
            //       perhaps by recursing into another function that handles comments
            if (directive_name == u8"\\comment" || directive_name == u8"\\-comment") {
                emit(i.n, Highlight_Type::comment_delimiter);
                ++in_comment;
            }
            else {
                emit(i.n, Highlight_Type::tag);
            }
            break;
        }
        case pop_directive: {
            --in_comment;
            break;
        }

        case argument_equal: // =
        case argument_comma: // ,
            emit(1, Highlight_Type::symbol);
            break;
        case push_arguments: // [
        case pop_arguments: // ]
        case push_block: // {
        case pop_block: // }
            emit(i.n, Highlight_Type::symbol_important);
            break;

        case push_document:
        case pop_document:
        case push_argument:
        case pop_argument: break;
        }
    }
}

} // namespace mmml

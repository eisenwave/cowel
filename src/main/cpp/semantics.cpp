#include <limits>
#include <string>
#include <vector>

#include "cowel/util/assert.hpp"

#include "cowel/ast.hpp"
#include "cowel/fwd.hpp"
#include "cowel/type.hpp"
#include "cowel/value.hpp"

namespace cowel {

static_assert(std::numeric_limits<Float>::is_iec559);
static_assert(sizeof(Float) == 8);

[[nodiscard]]
std::u8string Type::get_display_name() const
{
    std::u8string result;

    if (m_kind == Type_Kind::union_) {
        result += u8'(';
        bool first = true;
        for (const Type& m : m_members) {
            if (!first) {
                result += u8" | ";
            }
            first = false;
            result += m.get_display_name();
        }
        return result += u8')';
    }

    result += type_kind_display_name(get_kind());

    switch (m_members.size()) {
    case 0: break;
    case 1: {
        result += u8" ";
        result += m_members.front().get_display_name();
        break;
    }
    default: {
        result += u8'(';
        bool first = true;
        for (const Type& m : get_members()) {
            if (!first) {
                result += u8", ";
            }
            first = false;
            result += m.get_display_name();
        }
        result += u8')';
    }
    }

    return result;
}

Value Value::block(const ast::Primary& block, Frame_Index frame)
{
    COWEL_ASSERT(block.get_kind() == ast::Primary_Kind::block);
    return Value {
        Union { .block = Block_And_Frame { &block, frame } },
        block_index,
        &Type::block,
    };
}

Value Value::block(const ast::Directive& block, Frame_Index frame)
{
    // TODO: assertions
    return Value {
        Union { .directive = Directive_And_Frame { &block, frame } },
        directive_index,
        &Type::block,
    };
}

} // namespace cowel

#include <limits>
#include <ranges>
#include <span>
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
    return Value { Union { .block = Block_And_Frame { &block, frame } }, block_index };
}

Value Value::block(const ast::Directive& block, Frame_Index frame)
{
    // TODO: assertions
    return Value { Union { .directive = Directive_And_Frame { &block, frame } }, directive_index };
}

Value Value::dynamic_string_forced(const std::u8string_view value, const String_Kind kind)
{
    GC_Ref<char8_t> gc = gc_ref_from_range<char8_t>(value);
    return Value { Union { .dynamic_string = std::move(gc) }, dynamic_string_index, kind };
}

Value Value::string(const std::u8string_view value, const String_Kind kind)
{
    if (value.size() <= Short_String_Value::max_size_v) {
        return short_string({ value.data(), value.size() }, kind);
    }
    return dynamic_string_forced(value, kind);
}

Value Value::group(std::span<const Group_Member_Value> values)
{
    Group_Value gc = gc_ref_from_range<Group_Member_Value>(values);
    return Value { Union { .group = std::move(gc) }, group_index };
}

Value Value::group_move(std::span<Group_Member_Value> values)
{
    constexpr auto move_value
        = [](Group_Member_Value& v) -> Group_Member_Value&& { return std::move(v); };
    Group_Value gc
        = gc_ref_from_range<Group_Member_Value>(values | std::views::transform(move_value));
    return Value { Union { .group = std::move(gc) }, group_index };
}

Value Value::group_pack(std::span<const Value> values)
{
    constexpr auto copy_value = [](const Value& v) -> Group_Member_Value {
        return {
            .name = Value::null,
            .value = v,
        };
    };
    Group_Value gc
        = gc_ref_from_range<Group_Member_Value>(values | std::views::transform(copy_value));
    return Value { Union { .group = std::move(gc) }, group_index };
}

Value Value::group_pack_move(std::span<Value> values)
{
    constexpr auto move_value = [](Value& v) -> Group_Member_Value {
        return {
            .name = Value::null,
            .value = std::move(v),
        };
    };
    Group_Value gc
        = gc_ref_from_range<Group_Member_Value>(values | std::views::transform(move_value));
    return Value { Union { .group = std::move(gc) }, group_index };
}

static_assert(alignof(Value) <= 8, "Value should not be excessively aligned.");
static_assert(sizeof(Value) <= 64, "Value should not be too large to be passed by value.");

} // namespace cowel

#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ulight/json.hpp"

#include "cowel/util/assert.hpp"

#include "cowel/json.hpp"

namespace cowel::json {
namespace {
struct Building_Visitor final : ulight::JSON_Visitor {
    using Pos = ulight::Source_Position;

    std::optional<json::Value> root_value;
    std::pmr::vector<json::Value> structure_stack;
    std::pmr::vector<std::pmr::u8string> property_stack;

    std::pmr::u8string current_string;

    explicit Building_Visitor(std::pmr::memory_resource* memory)
        : structure_stack { memory }
        , property_stack { memory }
        , current_string { memory }
    {
    }

    void literal(const Pos&, std::u8string_view chars) final
    {
        current_string.append(chars);
    }

    void escape(const Pos&, std::u8string_view, char32_t, std::u8string_view code_units) final
    {
        current_string.append(code_units.begin(), code_units.end());
    }

    void number(const Pos&, std::u8string_view, double value) final
    {
        insert_value(value);
    }

    void null(const Pos&) final
    {
        insert_value(json::Null {});
    }
    void boolean(const Pos&, bool value) final
    {
        insert_value(value);
    }

    void push_string(const Pos&) final
    {
        current_string.clear();
    }
    void pop_string(const Pos&) final
    {
        insert_value(current_string);
    }

    void push_property(const Pos&) final
    {
        current_string.clear();
    }
    void pop_property(const Pos&) final
    {
        property_stack.push_back(current_string);
    }

    void push_object(const Pos&) final
    {
        structure_stack.push_back(json::Object {});
    }
    void pop_object(const Pos&) final
    {
        json::Value object = std::move(structure_stack.back());
        structure_stack.pop_back();
        insert_value(std::move(object));
    }

    void push_array(const Pos&) final
    {
        structure_stack.push_back(json::Array {});
    }
    void pop_array(const Pos&) final
    {
        json::Value array = std::move(structure_stack.back());
        structure_stack.pop_back();
        insert_value(std::move(array));
    }

    void insert_value(json::Value&& value)
    {
        if (structure_stack.empty()) {
            root_value = std::move(value);
            COWEL_ASSERT(property_stack.empty());
        }
        else if (auto* const array = std::get_if<json::Array>(&structure_stack.back())) {
            array->push_back(std::move(value));
        }
        else if (auto* const object = std::get_if<json::Object>(&structure_stack.back())) {
            object->push_back({ property_stack.back(), std::move(value) });
            property_stack.pop_back();
        }
    }
};
} // namespace

std::optional<json::Value> load(std::u8string_view source, std::pmr::memory_resource* memory)
{
    constexpr ulight::JSON_Options options { .allow_comments = true,
                                             .parse_numbers = true,
                                             .escapes = ulight::Escape_Parsing::parse_encode };
    Building_Visitor visitor { memory };
    if (!parse_json(visitor, source, options)) {
        return {};
    }
    return std::move(visitor.root_value);
}

} // namespace cowel::json

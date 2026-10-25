#include <algorithm>
#include <array>
#include <cstddef>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cowel/json.hpp"
#include "ulight/ulight.hpp"

#include "cowel/theme_to_css.hpp"

namespace cowel {
namespace {

struct Property {
    std::u8string_view key;
    std::u8string_view value;
};

struct Style {
    ulight::Highlight_Type type;
    std::pmr::vector<Property> properties;
};

struct Highlight_Lookup_Entry {
    std::u8string_view long_string;
    ulight::Highlight_Type type;
};

#define COWEL_HIGHLIGHT_TYPE_LOOKUP(id, long_string, short_string, value)                          \
    { u8##long_string, ulight::Highlight_Type::id },

constexpr auto highlight_type_lookup = []() {
    Highlight_Lookup_Entry result[] {
        ULIGHT_HIGHLIGHT_TYPE_ENUM_DATA(COWEL_HIGHLIGHT_TYPE_LOOKUP)
    };
    std::ranges::sort(result, {}, &Highlight_Lookup_Entry::long_string);
    return std::to_array(result);
}();

static_assert(
    std::ranges::is_sorted(highlight_type_lookup, {}, &Highlight_Lookup_Entry::long_string)
);
static_assert(
    std::ranges::adjacent_find(highlight_type_lookup, {}, &Highlight_Lookup_Entry::long_string)
        == highlight_type_lookup.end(),
    "highlight_type_lookup contains duplicate entries"
);

} // namespace

std::optional<ulight::Highlight_Type> highlight_type_by_long_string(std::u8string_view str)
{
    // NOLINTNEXTLINE(readability-qualified-auto)
    const auto it = std::ranges::lower_bound(
        highlight_type_lookup, str, {}, &Highlight_Lookup_Entry::long_string
    );
    if (it == std::end(highlight_type_lookup) || it->long_string != str) {
        return {};
    }
    return it->type;
}

namespace {

/// @brief Resolves a literal CSS value or a reference to a palette color (`"$..."`).
/// Palette colors may reference other palette colors,
/// so resolution recurses until it reaches a literal CSS value.
/// @param palette The optional palette containing named colors.
/// @param value The literal CSS value or `$name` reference to resolve.
/// @param resolution_stack Palette names currently being resolved, used to detect cycles.
std::optional<std::u8string_view> resolve_palette_reference(
    const json::Object* const palette,
    const std::u8string_view value,
    std::pmr::vector<std::u8string_view>& resolution_stack
)
{
    if (!value.starts_with(u8'$')) {
        return value;
    }
    if (!palette) {
        return {};
    }

    const std::u8string_view name = value.substr(1);
    const json::String* const palette_value = palette->find_string(name);
    if (!palette_value || std::ranges::contains(resolution_stack, name)) {
        return {};
    }

    resolution_stack.push_back(name);
    const auto result = resolve_palette_reference(palette, *palette_value, resolution_stack);
    resolution_stack.pop_back();
    return result;
}

bool decode_variant(
    std::pmr::vector<Style>& out,
    const json::Object& object,
    const json::Object* palette
)
{
    std::pmr::vector<Property> properties { out.get_allocator() };
    std::pmr::vector<std::u8string_view> resolution_stack { out.get_allocator() };
    for (const json::Member& member : object) {
        const std::optional<ulight::Highlight_Type> type
            = highlight_type_by_long_string(member.key);
        if (!type) {
            continue;
        }
        properties.clear();
        if (const auto* const string = member.value.as_string()) {
            const std::optional<std::u8string_view> color
                = resolve_palette_reference(palette, *string, resolution_stack);
            if (!color) {
                return false;
            }
            properties.emplace_back(u8"color", *color);
            out.emplace_back(*type, properties);
            continue;
        }
        if (const auto* const object = member.value.as_object()) {
            for (const json::Member& style_member : *object) {
                if (const auto* const str = style_member.value.as_string()) {
                    const std::optional<std::u8string_view> value
                        = resolve_palette_reference(palette, *str, resolution_stack);
                    if (!value) {
                        return false;
                    }
                    properties.emplace_back(style_member.key, *value);
                }
                else {
                    return false;
                }
            }
            out.emplace_back(*type, properties);
            continue;
        }
        return false;
    }

    return true;
}

void append(std::pmr::vector<char8_t>& out, std::u8string_view str)
{
    out.insert(out.end(), str.begin(), str.end());
}

void write_style(std::pmr::vector<char8_t>& out, const Style& style)
{
    append(out, u8"h-[data-h^=");
    append(out, ulight::highlight_type_short_string_u8(style.type));
    append(out, u8"] {");
    for (const Property& p : style.properties) {
        out.push_back(u8' ');
        append(out, p.key);
        out.push_back(u8':');
        append(out, p.value);
        append(out, u8";");
    }
    append(out, u8" }\n");
}

} // namespace

bool theme_to_css(
    std::pmr::vector<char8_t>& out,
    std::u8string_view theme_json,
    std::pmr::memory_resource* memory
)
{
    std::optional<json::Value> value = json::load(theme_json, memory);
    if (!value) {
        return false;
    }

    const auto* const root_object = value->as_object();
    if (!root_object) {
        return false;
    }

    std::pmr::vector<Style> light_styles;
    std::pmr::vector<Style> dark_styles;

    const json::Value* const palette_value = root_object->find_value(u8"palette");
    if (palette_value && !palette_value->as_object()) {
        return false;
    }
    const json::Object* const palette = palette_value ? palette_value->as_object() : nullptr;

    const auto* const light = root_object->find_object(u8"light");
    const auto* const dark = root_object->find_object(u8"dark");

    if (!light || !dark) {
        return false;
    }
    if (!decode_variant(light_styles, *light, palette)
        || !decode_variant(dark_styles, *dark, palette)) {
        return false;
    }

    append(out, u8"@media (prefers-color-scheme: light) {\n");
    for (const auto& style : light_styles) {
        write_style(out, style);
    }
    append(out, u8"}\n");

    append(out, u8"@media (prefers-color-scheme: dark) {\n");
    for (const auto& style : dark_styles) {
        write_style(out, style);
    }
    append(out, u8"}\n\n");

    for (const auto& style : light_styles) {
        append(out, u8"html.light ");
        write_style(out, style);
    }
    out.push_back(u8'\n');

    for (const auto& style : dark_styles) {
        append(out, u8"html.dark ");
        write_style(out, style);
    }

    return true;
}

} // namespace cowel

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "mmml/json.hpp"
#include "ulight/ulight.hpp"

#include "mmml/theme_to_css.hpp"

namespace mmml {
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

#define MMML_HIGHLIGHT_TYPE_LOOKUP(id, long_string, short_string, value)                           \
    { u8##long_string, ulight::Highlight_Type::id },

constexpr auto highlight_type_lookup = []() {
    Highlight_Lookup_Entry result[] { ULIGHT_HIGHLIGHT_TYPE_ENUM_DATA(MMML_HIGHLIGHT_TYPE_LOOKUP) };
    std::ranges::sort(result, {}, &Highlight_Lookup_Entry::long_string);
    return std::to_array(result);
}();

[[nodiscard]]
constexpr std::optional<ulight::Highlight_Type> highlight_type_by_long_string(std::u8string_view str
)
{
    const auto* const it = std::ranges::lower_bound(
        highlight_type_lookup, str, {}, &Highlight_Lookup_Entry::long_string
    );
    if (it == std::end(highlight_type_lookup) || it->long_string != str) {
        return {};
    }
    return it->type;
}

bool decode_variant(std::pmr::vector<Style>& out, const json::Object& object)
{
    std::pmr::vector<Property> properties { out.get_allocator() };
    for (const json::Member& member : object) {
        const std::optional<ulight::Highlight_Type> type
            = highlight_type_by_long_string(member.key);
        if (!type) {
            continue;
        }
        properties.clear();
        if (const auto* const string = member.value.as_string()) {
            properties.emplace_back(u8"color", *string);
            out.emplace_back(*type, properties);
            continue;
        }
        if (const auto* const object = member.value.as_object()) {
            for (const json::Member& style_member : *object) {
                if (const auto* const str = style_member.value.as_string()) {
                    properties.emplace_back(style_member.key, *str);
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

    const auto* const light = root_object->find_object(u8"light");
    const auto* const dark = root_object->find_object(u8"dark");

    if (!light || !dark) {
        return false;
    }
    if (!decode_variant(light_styles, *light) || !decode_variant(dark_styles, *dark)) {
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

} // namespace mmml

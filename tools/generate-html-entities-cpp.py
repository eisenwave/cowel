#!/bin/python
import json
import sys

data = json.load(sys.stdin)

def format_code_points(code_points) -> str:
    formatted_code_points = []
    for code_point in code_points:
        if code_point > 0xFFFF:
            formatted_code_points.append(f"U'\\U{code_point:08X}'")
        else:
            formatted_code_points.append(f"U'\\u{code_point:04X}'")
    return ",".join(formatted_code_points)

output = []

for name, entry in data.items():
    name_cropped = name[1:-1]
    code_points_formatted = format_code_points(entry["codepoints"])
    output.append(f'{{u8"{name_cropped}",{len(name_cropped)},{{{code_points_formatted}}}}},')

output.sort()
print("// NOLINTBEGIN")
print("// clang-format off")
print("\n".join(output))
print("// clang-format on")
print("// NOLINTEND")

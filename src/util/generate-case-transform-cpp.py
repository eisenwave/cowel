#!/usr/bin/env python3

# This script converts from the input data given in
# https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt
# to a brace-enclosed list of character literals suitable for use in C++.
# The output is sorted.
#
# It also converts from the input data given in
# https://www.unicode.org/Public/17.0.0/ucd/SpecialCasing.txt
# to a brace-enclosed list of character literals suitable for use in C++.
# The output needs sorting by piping into sort.

import sys

transformations = ["simple_lower", "simple_upper", "special_lower", "special_upper"]

def to_char_escape(code_point: int) -> str:
    return f"\\U{code_point:08X}" if code_point > 0xFFFF else f"\\u{code_point:04X}"

def to_char_literal(code_point: int) -> str:
    return f"U'{to_char_escape(code_point)}'"

def format_code_points(code_points) -> str:
    return ",".join(to_char_literal(c) for c in code_points)

def print_simple(code_point, transformed):
    print(f"{{{format_code_points([code_point, transformed])}}},")

def print_special(code_point, transformed: list[int]):
    key = to_char_literal(code_point)
    value = f"U\"{"".join(to_char_escape(c) for c in transformed)}\""
    print(f"{{{key},{value}}},")

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <Data.txt> ({"|".join(transformations)})", file=sys.stderr)
        sys.exit(1)

    transformation = sys.argv[2]
    if transformation not in transformations:
        print(f"Invalid transformation \"{transformation}\"", file=sys.stderr)
        sys.exit(1)
    expected_columns = 15 if transformation.startswith("simple") else 5

    path = sys.argv[1]

    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue

            fields = line.split(";")

            if len(fields) < expected_columns:
                print(f"Expected at least {expected_columns} columns, but got {len(fields)}", file=sys.stderr)
                sys.exit(1)
            if transformation.startswith("special") and len(fields) >= 6:
                # Beginning of conditional or language-specific mappings,
                # which we don't handle (yet).
                break

            code_point = int(fields[0], 16)

            match transformation:
                case "simple_upper":
                    general_category = fields[2]
                    # There exist certain characters such as LATIN SMALL LETTER SHARP S
                    # that don't have a simple mapping to uppercase.
                    if general_category == "Ll" and fields[12]:
                        print_simple(code_point, int(fields[12], 16))
                case "simple_lower":
                    general_category = fields[2]
                    if general_category == "Lu" and fields[13]:
                        print_simple(code_point, int(fields[13], 16))
                case "special_upper":
                    transformed = [int(c, 16) for c in fields[3].strip().split(" ")]
                    if len(transformed) >= 2:
                        print_special(code_point, transformed)
                case "special_lower":
                    transformed = [int(c, 16) for c in fields[1].strip().split(" ")]
                    if len(transformed) >= 2:
                        print_special(code_point, transformed)
                case _:
                    sys.exit(1)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3

# This script converts from the input data given in
# https://www.unicode.org/Public/17.0.0/ucd/DerivedCoreProperties.txt
# to brace-enclosed range literals suitable for use in C++.
#
# Usage: generate-unicode-case-properties-cpp.py <DerivedCoreProperties.txt> (cased|case_ignorable)
#
# Each output line has the form:
#   {U'\uXXXX',U'\uXXXX'},
# representing an inclusive range [from, to] of code points
# that have the given property.
# The output is sorted in ascending order by the 'from' code point.

import sys

properties = ["cased", "case_ignorable"]

property_names = {
    "cased": "Cased",
    "case_ignorable": "Case_Ignorable",
}


def to_char_escape(code_point: int) -> str:
    return f"\\U{code_point:08X}" if code_point > 0xFFFF else f"\\u{code_point:04X}"


def to_char_literal(code_point: int) -> str:
    return f"U'{to_char_escape(code_point)}'"


def print_range(from_cp: int, to_cp: int) -> None:
    print(f"{{{to_char_literal(from_cp)},{to_char_literal(to_cp)}}},")


def main() -> None:
    if len(sys.argv) != 3:
        print(
            f"Usage: {sys.argv[0]} <DerivedCoreProperties.txt>"
            f" ({"|".join(properties)})",
            file=sys.stderr,
        )
        sys.exit(1)

    prop_key = sys.argv[2].lower()
    if prop_key not in properties:
        print(f'Invalid property "{sys.argv[2]}"', file=sys.stderr)
        sys.exit(1)

    prop_name = property_names[prop_key]
    path = sys.argv[1]

    ranges: list[tuple[int, int]] = []

    with open(path, encoding="utf-8") as f:
        for line in f:
            # Strip comments and whitespace
            line = line.split("#")[0].strip()
            if not line:
                continue

            fields = [f.strip() for f in line.split(";")]
            if len(fields) < 2:
                continue

            if fields[1] != prop_name:
                continue

            # Parse code point or range
            cp_field = fields[0]
            if ".." in cp_field:
                parts = cp_field.split("..")
                from_cp = int(parts[0], 16)
                to_cp = int(parts[1], 16)
            else:
                from_cp = int(cp_field, 16)
                to_cp = from_cp

            ranges.append((from_cp, to_cp))

    # The file is already sorted, but verify and sort to be safe.
    ranges.sort(key=lambda r: r[0])

    for from_cp, to_cp in ranges:
        print_range(from_cp, to_cp)


if __name__ == "__main__":
    main()

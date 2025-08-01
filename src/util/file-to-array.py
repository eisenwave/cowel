#!/bin/python

import sys

def emit_cpp_array(in_path, outfile, var_name):
    with open(in_path, "rb") as input:
        data = input.read()

    with open(outfile, "w", encoding="utf-8") as output:
        output.write("#include <cstddef>\n")
        output.write("#include <string_view>\n\n")
        output.write("#include \"cowel/assets.hpp\"\n\n")
        output.write("namespace cowel::assets {\n")
        output.write("namespace {\n")
        output.write(f"constexpr char8_t {var_name}_data[] = {{\n    ")

        for i, b in enumerate(data):
            output.write(f"0x{b:02x}, ")
            if (i + 1) % 16 == 0:
                output.write("\n    ")

        output.write("\n};\n")
        output.write(f"constexpr std::size_t {var_name}_size = sizeof({var_name}_data);\n")
        output.write("}\n")
        output.write(f"constinit const std::u8string_view {var_name} {{ {var_name}_data, {var_name}_size }};\n")

        output.write("}\n")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"Usage: {sys.argv[0]} IN_FILE OUT_FILE VAR_NAME")
        exit(1)
    in_path = sys.argv[1]
    out_path = sys.argv[2]
    var_name = sys.argv[3]
    emit_cpp_array(in_path, out_path, var_name)

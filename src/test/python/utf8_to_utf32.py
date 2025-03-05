#!/usr/bin/python
import sys

def convert_utf8_to_utf32le(in_path, out_path):
    with open(in_path, 'r', encoding='utf-8') as in_file:
        content = in_file.read()
    
    with open(out_path, 'w', encoding='utf-32-le') as out_file:
        out_file.write(content)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python convert_encoding.py <input_file> <output_file>")
        sys.exit(1)
    
    input_filename = sys.argv[1]
    output_filename = sys.argv[2]

    convert_utf8_to_utf32le(input_filename, output_filename)

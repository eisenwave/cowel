#!/bin/bash


# -o -name "*.hpp" would also process headers
FILES=$(find src -type f \( -name "*.cpp" \))
COMMAND="clang-tidy-19 $FILES -p build"

echo $COMMAND
$COMMAND

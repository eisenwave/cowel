#!/bin/bash

SOURCES=$(find src -type f \( -name "*.cpp" -o -name "*.hpp" \))
INCLUDES=$(find include -type f \( -name "*.hpp" \))

COMMAND="clang-tidy-19 $SOURCES $INCLUDES -p build"

echo $COMMAND
$COMMAND

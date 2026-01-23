#!/bin/bash
DIRECTORY="third_party/boost"
git clone https://github.com/boostorg/boost.git "$DIRECTORY"
(
  cd "$DIRECTORY" || exit 1
  git submodule update --init \
  tools/build \
  libs/config \
  libs/multiprecision
  ./bootstrap.sh
)

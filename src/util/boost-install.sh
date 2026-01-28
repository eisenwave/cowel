#!/bin/bash

if [[ $# -eq 0 ]]; then
  echo "Error: Boost installation argument required (usually third_party/boost)"
  echo "Usage: $0 <directory>"
  exit 1
fi

BOOST_URL="https://github.com/boostorg/boost.git"
DIRECTORY="$1"

git clone "$BOOST_URL" "$DIRECTORY"
(
  cd "$DIRECTORY" || exit 1
  git submodule update --init \
  tools/build \
  libs/config \
  libs/multiprecision
  ./bootstrap.sh
)

#!/bin/bash

set -eou pipefail

if [[ $# -eq 0 ]]; then
  echo "Error: Boost installation argument required (usually third_party/boost)"
  echo "Usage: $0 <directory>"
  exit 1
fi

BOOST_URL="https://github.com/boostorg/boost.git"
BOOST_BRANCH="boost-1.90.0"
DIRECTORY="$1"

git clone "$BOOST_URL" \
  --branch "$BOOST_BRANCH" \
  --depth 1 \
  --single-branch \
  "$DIRECTORY"
(
  cd "$DIRECTORY" || exit 1
  git submodule update --init \
  tools/build \
  libs/config \
  libs/multiprecision
  ./bootstrap.sh
)

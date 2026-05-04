#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/clang20-coverage"
COVERAGE_DIR="${BUILD_DIR}/coverage"
PROFDATA_FILE="${COVERAGE_DIR}/coverage.profdata"
LCOV_FILE="${COVERAGE_DIR}/lcov.info"

select_tool() {
    local base_name="$1"
    if command -v "${base_name}-20" >/dev/null 2>&1; then
        echo "${base_name}-20"
        return
    fi

    if command -v "${base_name}" >/dev/null 2>&1; then
        echo "${base_name}"
        return
    fi

    echo "Missing required tool: ${base_name}" >&2
    exit 1
}

LLVM_PROFDATA_TOOL="$(select_tool llvm-profdata)"
LLVM_COV_TOOL="$(select_tool llvm-cov)"

cd "${REPO_ROOT}"

echo "Configuring clang20-coverage preset"
cmake --preset clang20-coverage

echo "Building clang20-coverage preset"
cmake --build --preset clang20-coverage -j4

echo "Running tests with profile output"
rm -rf "${COVERAGE_DIR}"
mkdir -p "${COVERAGE_DIR}"
LLVM_PROFILE_FILE="${COVERAGE_DIR}/cowel-test-%p.profraw" \
ctest --preset clang20-coverage -L cowel

mapfile -t profraw_files < <(find "${COVERAGE_DIR}" -type f -name '*.profraw' | sort)
if [[ ${#profraw_files[@]} -eq 0 ]]; then
    echo "No .profraw files were generated" >&2
    exit 1
fi

echo "Merging profile data"
"${LLVM_PROFDATA_TOOL}" merge -sparse "${profraw_files[@]}" -o "${PROFDATA_FILE}"

echo "Exporting LCOV"
"${LLVM_COV_TOOL}" export \
    -format=lcov \
    -ignore-filename-regex='/(third_party|ulight|build|engine/test/src)/' \
    -instr-profile="${PROFDATA_FILE}" \
    "${BUILD_DIR}/cowel-test" > "${LCOV_FILE}"

echo "Filtering LCOV exclusions"
python3 "${REPO_ROOT}/tools/filter-lcov.py" "${LCOV_FILE}"

echo "Coverage file generated: ${LCOV_FILE}"

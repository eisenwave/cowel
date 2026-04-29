# Copilot Cloud Agent Onboarding

Trust this document first.
Only search the repository when information here is missing or proves incorrect.

## Repository Summary

- `cowel` is a C++23 + Node.js project for **Compact Web Language (COWEL)**,
  a TeX-like markup language that compiles to HTML.
- The native product is a CLI (`cowel-cli`) and static library (`cowel`).
- The npm product is a Node CLI backed by a WASM build (`cowel-npm` target, outputs to `build/npm`).
- Main languages:
  - C++ (core compiler)
  - TypeScript/JavaScript (npm CLI/tests),
  - COWEL documents (`.cow` or `.cowel`),
  - Python build helpers
- Approximate size: medium-large monorepo with embedded/third-party content
  (`third_party/boost`, `ulight/`, generated build trees).

## Documentation Style

- The project uses semantic line breaks for comments and documentation:
  https://sembr.org/
- When editing Markdown, prose comments, or long documentation strings,
  write one semantic unit per line and reflow changed prose to this style.
- Apply semantic line breaks as a transform,
  not only for new text but also for touched surrounding prose.

## High-Value Layout (Start Here)

- Root build system: `CMakeLists.txt`
- Core C++ headers: `include/cowel/`
- Core C++ sources: `src/main/cpp/`
- C++ tests: `src/test/cpp/`
- TS sources: `src/main/ts/`
- TS tests: `src/test/ts/`
- Utility scripts: `src/util/`
- Docs + golden sample I/O: `docs/index.cow` and `docs/index.html`
- VS Code extension + TextMate grammar tests: `editor/vscode/`
- CI workflows:
  - `.github/workflows/cmake-multi-platform.yml`
  - `.github/workflows/clang-format.yml`
  - `.github/workflows/textmate-test.yml`

Important dependency facts not obvious from tree:
- Native configure requires ICU (`find_package(ICU COMPONENTS data i18n uc REQUIRED)`).
- Native configure requires Python 3 (`find_package(Python3 REQUIRED)`),
  used to embed assets (`src/util/file-to-array.py`).
- If `third_party/boost` is missing, configure auto-clones Boost via `src/util/boost-install.sh`.
- `ulight` is a git submodule and is built as part of the top-level CMake project.

## Toolchain Versions Validated Locally

- Node.js `v20.19.6`
- npm `10.8.2`
- CMake/CTest `4.3.2`
- Python `3.12.3`
- GCC detected by CMake: `13.3.0`

CI also uses:
- Node 20
- gcc-13 and clang-20 variants
- clang-format-20
- Emscripten toolchain for WASM path

## Always-Use Command Order (Native)

Run from repo root.

1. Bootstrap/preconditions

```bash
git submodule update --init --recursive
npm install
```

2. Configure clean native build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

3. Build native targets

```bash
cmake --build build --config Debug -j4
```

4. Run C++ tests

```bash
ctest --test-dir build --output-on-failure
```

5. Run CLI golden validation

```bash
./build/cowel-cli run docs/index.cow build/docs.actual.html
diff -u docs/index.html build/docs.actual.html
```

### Validated outcomes/timings

- Building before configure fails fast
  (`Error: .../build is not a directory`, ~0.14s).
  Always configure first.
- Configure succeeded (~11.8s).
- Full native build succeeded (~57.8s).
- Native tests succeeded: `332/332` passed
  (~47.8s wall clock; CTest real ~45.8s).
- CLI docs golden diff succeeded (no diff, ~0.27s).

## Node/TypeScript Validation

From repo root:

```bash
npm install
npm run build
npm run build:test
npx eslint src --max-warnings=0 --color
npm test
```

Validated outcomes/timings:
- `npm run build` succeeded (~0.82s)
- `npm run build:test` succeeded (~1.09s)
- ESLint command succeeded (~2.06s)
- `npm test` succeeded (`96` pass, `0` fail, ~0.30s)

## VS Code Grammar Validation (CI Parity)

From `editor/vscode/`:

```bash
npm install
npm test
```

Validated outcomes/timings:
- install succeeded (~3.55s)
- tests succeeded
  (`35` passed, `1` fixture marked "without expectations", ~1.17s)

## Formatting Gate (CI Parity)

CI command (run from repo root):

```bash
find include src -name '*.cpp' -o -name '*.c' -o -name '*.hpp' -o -name '*.h' |
  xargs clang-format-20 --color=1 --dry-run --Werror
```

Validated outcome:
- command succeeds when `clang-format-20` is available (~1.19s).

## WASM/NPM CMake Path

CI uses Emscripten and builds target `cowel-npm`.

Expected sequence:
1. Install and activate emsdk.
2. Configure with Emscripten toolchain file.
3. Build `cowel-npm` target.
4. Run `npm test` and CLI smoke test.

Observed failure when emsdk is absent:
- `Could not find toolchain file: emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`
- plus compiler/make-program detection failures
- fails quickly (~0.14s)

Mitigation:
- always install/activate emsdk before wasm configure,
  and verify toolchain file path exists.

## Architecture Pointers For Fast Edits

- CLI entrypoint: `src/main/cpp/cli.cpp`
- Parse/lex pipeline: `src/main/cpp/lex.cpp`, `src/main/cpp/parse.cpp`, `src/main/cpp/build_ast.cpp`
- Directive implementations: `src/main/cpp/directives/*.cpp`
- Builtin directive wiring: `src/main/cpp/builtin_directive_set.cpp`, `include/cowel/builtin_directive_set.hpp`
- Document generation: `src/main/cpp/document_generation.cpp`
- Services/settings/context: `src/main/cpp/services.cpp`, `include/cowel/settings.hpp`, `include/cowel/context.hpp`
- C++ tests focused by behavior: `src/test/cpp/test_*.cpp`

## Pre-PR Validation Checklist (Replicate CI)

Always run the smallest relevant subset first,
then broaden:

1. Build/test impacted native code:
```bash
cmake --build build --config Debug -j4
ctest --test-dir build --output-on-failure
```

2. If TS/JS touched:
```bash
npm run build
npm run build:test
npx eslint src --max-warnings=0 --color
npm test
```

3. If grammar/editor files touched:
```bash
cd editor/vscode && npm test
```

4. Before finalizing C++ changes:
```bash
find include src -name '*.cpp' -o -name '*.c' -o -name '*.hpp' -o -name '*.h' |
  xargs clang-format-20 --color=1 --dry-run --Werror
```

## Root Inventory (Quick Reference)

Top-level entries include:
```
.github
.vscode
assets
docs
editor
include
src
test
third_party
ulight
CMakeLists.txt
package.json
eslint.config.js`
tsconfig*.json
README.md
CONTRIBUTING.md
CHANGELOG.md
```

## Practical Guardrails

- Always run `npm install` before JS/TS lint/test/build workflows.
- Always configure CMake before any `cmake --build` or `ctest` on a new build directory.
- Prefer out-of-tree build dirs (for example `build/`) to avoid polluting tracked files.
- For native parity with CI, keep warnings clean and run tests with `--output-on-failure`.
- Treat this file as primary operating guidance;
  search only for missing details or when commands here no longer match repository reality.

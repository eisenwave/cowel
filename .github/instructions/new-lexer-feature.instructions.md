# New Lexer Feature Instructions

Use this when adding new token kinds, new lexer productions,
or modifying the tokenization behavior of COWEL source files.

## Relevant Files and Directories

| Path | Purpose |
|------|---------|
| `engine/include/cowel/syntax/lex.hpp` | Token kind enum (`COWEL_TOKEN_KIND_ENUM_DATA` macro), `Token` struct, `lex()` declaration |
| `engine/src/syntax/lex.cpp` | `Lexer` struct with all lexing methods |
| `engine/test/src/test_lexing.cpp` | Test runner; also contains `token_kind_source()` and `token_kind_name()` which must be updated |
| `engine/test/files/lex/` | File-based lex fixtures; see [engine/test/files/README.md](../../engine/test/files/README.md) for the full fixture format |
| `docs/intro/grammar.ebnf` | Normative EBNF grammar — update the lexical grammar rules here when adding tokens |

Secondary files that reference token kinds and may need updating:

- `engine/src/syntax/parse.cpp` — switch statements on `Token_Kind`;
  add cases or the compiler will warn (`-Wswitch`)
- `engine/src/syntax/build_ast.cpp` — may reference specific token kinds

## Token Kind Enum Rules

Entries in `COWEL_TOKEN_KIND_ENUM_DATA` in `lex.hpp` must be
**alphabetically sorted by the string name** (second macro argument).
The test infrastructure has a `static_assert` that enforces this.

Each entry has the form:
```cpp
F(cpp_id, "ENUM-NAME", first_char)
```
- `cpp_id`
  C++ enum enumerator name.
- `"ENUM-NAME"`
  string used in `.lextest` files and test output.
- `first_char`
  expected first byte of the token (used in debug assertions);
  use `'\0'` when the first byte varies.

## General Procedure

1. **Grammar first**:
   Update `docs/intro/grammar.ebnf` to reflect the new token or production,
   following the existing EBNF style.
   It is also possible that the grammar was manually modified by the user
   and your task is to implement it.

2. **Add the token kind**:
   Insert a new `F(...)` line in `COWEL_TOKEN_KIND_ENUM_DATA` in `lex.hpp`,
   preserving alphabetical order by string name.

3. **Update `token_kind_source`** in `engine/test/src/test_lexing.cpp`:
   add a `case` for the new kind returning its fixed source text (`u8"..."sv`),
   or returning `{}` if the text varies per token.

4. **Implement lexing** in `lex.cpp`:
   Depends on the feature in question.
   Follow the existing patterns, and keep the existing changes minimal.
   Small changes in the grammar should also result in small changes to the code.

5. **Write fixture tests**:
   Create a single file or subdirectory under `engine/test/files/lex/`
   containing source and expectation file pairs.
   The test runner discovers all `.cow` files with matching `.cow.lextest`
   files automatically; no registration is needed.
   See [engine/test/files/README.md](../../engine/test/files/README.md)
   for the complete fixture format.

7. **Regenerate the docs golden file**:
   Any change to `docs/intro/grammar.ebnf`
   (or any other file included by `docs/index.cow`) changes the HTML output.
   Update the golden file before running `Document_Generation` tests:
   ```bash
   ./build/cowel-cli run docs/index.cow docs/index.html
   ctest --test-dir build --output-on-failure -R Document_Generation
   ```

6. **Rebuild and run tests**:
   ```bash
   cmake --build build --config Debug -j8
   ctest --test-dir build --output-on-failure -R Lex
   ```

8. **Fix parser warnings**:
   Adding a token kind triggers `-Wswitch` warnings
   in `parse.cpp` and `build_ast.cpp`.
   Add a `case` or `default` as appropriate.

## Key Implementation Patterns

### Backslash-prefixed tokens
Wire into `consume_backslash_prefixed()`:
```cpp
const bool non_escape_matched
    = expect_line_comment()
    || expect_block_comment()
    || expect_expression_splice()
    || expect_my_new_feature()   // new token added here
    || expect_directive_splice();
```
Each `expect_*` function checks a unique prefix so ordering only
affects readability, not correctness.

### Debug first-char assertion
The `emit()` method in debug builds asserts that `token_kind_first_char(kind)`
matches the current source byte.
Set the `first_char` in `COWEL_TOKEN_KIND_ENUM_DATA` to `'\0'`
to skip the check when the first byte varies.

## Clang-Format Gate

After any `.cpp`/`.hpp` change, ensure the formatting gate passes:
```bash
find engine/include engine/src bindings/native/src bindings/node/src/cpp \
  \( -name '*.cpp' -o -name '*.c' -o -name '*.hpp' -o -name '*.h' \) |
  xargs clang-format-20 --color=1 --dry-run --Werror
```

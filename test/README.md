# Test Fixtures

This directory contains data-driven test fixtures for the COWEL lexer and parser.
Tests are driven by C++ code in `src/test/cpp/`.

## Lexer Tests (`test/lex/`)

Each lexer test consists of a pair of files:

- **`<name>.cow`** — the COWEL source input.
- **`<name>.cow.lextest`** — the expected token sequence.

The test runner (`Lex.file_tests` in `test_lexing.cpp`)
discovers every `.cow` file recursively under `test/lex/`
and runs it against the matching `.cow.lextest`, if one exists.
Tests run from the repository root.

### `.lextest` format

Each non-blank line specifies one expected token:

```
TOKEN-KIND-NAME "text"
```

- `TOKEN-KIND-NAME` is the uppercase hyphenated name
  from `COWEL_TOKEN_KIND_ENUM_DATA` (e.g. `DOCUMENT-TEXT`, `BRACE-LEFT`).
- For **variable-text** tokens (e.g. `DOCUMENT-TEXT`, `IDENTIFIER`,
  `DIRECTIVE-SPLICE-NAME`) the double-quoted text is required.
- For **fixed-text** tokens whose text is implied by the kind
  (e.g. `BRACE-LEFT`, `PARENTHESIS-RIGHT`, `COMMA`, `ELLIPSIS`)
  the quoted argument may be omitted.
- Blank lines are ignored and serve as visual separators.
- Non-printing characters in quoted text use `\uXXXX` / `\UXXXXXXXX` escapes;
  backslash is `\\`.

Example — `test/lex/hello_world.cow.lextest`:
```
DOCUMENT-TEXT "Hello world!\u000a"
```

A `RESERVED-ESCAPE` or `RESERVED-NUMBER` token in a lextest
marks the test as expecting lexing to **fail**.
An empty `.cow.lextest` asserts that the source produces no tokens.
On failure,
the test prints the expected and actual sequences followed by a unified diff.

## Parser Tests (`test/parse/`)

Each file-driven parser test consists of a pair:

- **`<name>.cow`** — the COWEL source input.
- **`<name>.expected`** — the expected CST instruction sequence.

Tests are registered as `TEST(Parse, …)` macros in `test_parsing.cpp`.
Each calls `run_parse_test("name.cow", "name.expected")`,
prepending `test/parse/` to both paths.
The test runner must be invoked from the repository root.

### `.expected` format

Each line is a CST instruction kind name from `COWEL_CST_INSTRUCTION_KIND_ENUM_DATA`
(lowercase with underscores),
optionally followed by a space and an integer operand:

```
push_document 2
push_directive_splice
push_group 2
push_named_member
unquoted_member_name
skip
equals
skip
unquoted_string
pop_named_member
pop_group
pop_directive_splice
pop_document
```

The instructions with an integer operand are:
`push_document N` (N = top-level elements),
`push_group N` (N = group members),
`push_block N` (N = block elements), and
`push_quoted_string N` (N = string elements).

`skip` means a token (whitespace, comma, `=`) is consumed
but contributes no AST node.

### `Parse_And_Build` tests

`TEST(Parse_And_Build, …)` macros validate the full AST
after `parse` + `build_ast`.
They are written entirely in C++ (no `.expected` files)
and compare constructed `Node` trees directly
against the output of `parse_and_build_file`.
On failure both test kinds print a unified diff.

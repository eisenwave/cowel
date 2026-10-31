# Auto-Fix Test Fixtures

This directory contains data-driven test fixtures for auto-fixes.
Tests are driven by C++ code in `engine/test/src/test_autofix.cpp`.

Each auto-fix test consists of a pair of files:

- **`<name>.cow`** — the COWEL source input.
- **`<name>.diff`** — the expected auto-fixes,
  in unified diff format, with `a/` and `b/` prefixes.
  An empty file expresses that no auto-fixes are expected.

The test runner (`Autofix.file_tests` in `test_autofix.cpp`)
discovers every `.cow` file recursively under `engine/test/autofix/`,
and requires the matching `.diff` to exist.
Tests run from the repository root.

The runner applies the `.diff` to the `.cow` source,
and compares the result to the fixed source
produced by `COWEL_GEN_FLAGS_FIX_ALL`
(the `--fix-all` command line option).
In addition, the runner verifies that

- processing succeeds,
- only `deprecated` warnings are emitted, and
- processing the fixed source produces the same document as processing the original source.

An empty `.diff` is therefore used to express
that a document contains deprecated directives which cannot be converted automatically,
resulting in a bailout rather than an auto-fix.

## Adding A Test

Add the source file, then generate the expected diff from the actual auto-fixes:

```
./build/cowel-cli run --fix-all engine/test/autofix/example.cow /tmp/example.fixed.cow
diff -u --label a/example.cow --label b/example.cow \
  engine/test/autofix/example.cow /tmp/example.fixed.cow \
  > engine/test/autofix/example.diff
```

Review the generated diff before committing it.

## Format

Patches must be applicable with `patch -p1`,
which implies the following restrictions:

- Files must end with a newline,
  so that diffs never contain a `\ No newline at end of file` marker.
- Hunk headers must use the standard `@@ -old_start,old_count +new_start,new_count @@` form,
  optionally followed by section text which the test runner ignores.

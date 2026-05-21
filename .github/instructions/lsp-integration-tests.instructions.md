---
description: "Use when debugging or fixing COWEL LSP server behavior, adding LSP integration tests, or working in bindings/lsp/ or bindings/test/lsp/. Covers Server_State architecture, include-chain revalidation, fixture file format, placeholder syntax, diagnostic codes, and test validation."
applyTo: ["bindings/lsp/**", "bindings/test/lsp/**"]
---

# COWEL LSP Server: Debugging, Fixing, and Testing

## LSP Server Architecture

The LSP server lives in a single file: `bindings/lsp/lsp.cpp`.
It is compiled to WASM and consumed by `bindings/node/src/lsp-wasm-runner.ts`.
It implements a subset of the LSP protocol over JSON-RPC.

### Key types and state in `Server_State`

| Field | Type | Purpose |
|---|---|---|
| `open_docs` | `String_Map<std::u8string>` | Maps URI → current text for all open documents |
| `doc_includes` | `String_Map<std::vector<std::u8string>>` | Maps entry-point URI → transitive include closure (all URIs included during last validation) |
| `client_capabilities` | `Client_Capabilities` | Negotiated at initialize |

### Key functions

- `validate_document(uri, content, context, included_uris_out)` —
  Runs COWEL compilation on `content`.
  If `included_uris_out` is non-null,
  it is populated with the URI of every file in `validation_context.includes`
  (the full transitive closure).

- `publish_diagnostics_for(doc_text, context)` —
  Calls `validate_document`,
  publishes `textDocument/publishDiagnostics`,
  and stores the include closure into `doc_includes`.

- `revalidate_includers(changed_uri, context)` —
  Scans `doc_includes` for all open entry-point URIs V
  such that `doc_includes[V]` contains `changed_uri` and V ≠ `changed_uri`.
  Re-validates each such V by calling `publish_diagnostics_for`.
  Must collect the list before iterating to avoid iterator invalidation.

### Config vs. no-config validation paths

- **Config case** (`.cowel_config.json` has an `"include"` key):
  `find_config_entry_points(uri)` finds the same config for all files in the project.
  Every `didChange` already re-validates all config entry points,
  so includers are covered automatically.

- **No-config / standalone case** (`.cowel_config.json` is `{}` or absent):
  Only the changed file itself was re-validated before issue #370 fix.
  `revalidate_includers` is called in `handle_did_open` and `handle_did_change`
  to cover this path.

## Debugging Include-Chain Update Bugs

When an includer document A shows stale diagnostics after included file B changes:

1. Confirm it is the **no-config** case
   (`.cowel_config.json` has no `"include"` array).
2. Check whether `revalidate_includers` is called after `publish_diagnostics_for`
   in `handle_did_change` (and `handle_did_open`).
3. Verify that `doc_includes` is populated in `publish_diagnostics_for`:
   the `included_uris_out` parameter must be passed and stored with `insert_or_assign`.
4. Verify `revalidate_includers` correctly looks up `doc_includes` and finds A's entry.
5. Use the integration test fixture `bindings/test/lsp/include_update_clears_error/`
   as a regression test for this bug pattern.

## LSP Integration Test Structure

Each test suite lives in its own subdirectory under `bindings/test/lsp/`.

### Required files

| File | Purpose |
|---|---|
| `.cowel_config.json` | Project config; use `{}` for standalone (no-config) tests |
| `input.json` | Ordered array of LSP request/notification messages to send |
| `output.json` | Ordered array of expected LSP notification messages to receive |
| ≥1 `.cow` file | COWEL source file(s) referenced by the messages |

### Trailing newline rule

**Every `.cow` and JSON file in a fixture MUST end with a newline (`\n`).**
This applies to:
- All `.cow` source files (`main.cow`, `lib.cow`, etc.)
- `.cowel_config.json`
- `input.json`
- `output.json`

Create all fixture files with a final `\n`.
Verify with:
```bash
find bindings/test/lsp/<suite> -type f | while read f; do
  [ "$(tail -c1 "$f" | wc -l)" -eq 0 ] && echo "MISSING: $f" || echo "OK: $f"
done
```

### Placeholder syntax (substituted at test load time)

| Placeholder | Expands to |
|---|---|
| `{{ROOT_URI}}` | `file://` URI of the suite directory |
| `{{ROOT_PATH}}` | Filesystem path of the suite directory |
| `{{TEXT:filename}}` | Contents of `filename` in the suite directory |

`{{TEXT:filename}}` is substituted **after** JSON parsing,
so file contents with backslashes are safe inside JSON strings.

### `input.json` format

An array of JSON-RPC 2.0 notification/request objects.
Common methods:

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/didOpen",
  "params": {
    "textDocument": {
      "uri": "{{ROOT_URI}}/main.cow",
      "languageId": "cowel",
      "version": 1,
      "text": "{{TEXT:main.cow}}"
    }
  }
}
```

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/didChange",
  "params": {
    "textDocument": {
      "uri": "{{ROOT_URI}}/lib.cow",
      "version": 2
    },
    "contentChanges": [
      { "text": "\\cowel_macro(\"m\"){Hello}" }
    ]
  }
}
```

Note: use `\\` in JSON string literals to embed a literal backslash.

### `output.json` format

An array of expected `textDocument/publishDiagnostics` notifications (in order):

```json
{
  "jsonrpc": "2.0",
  "method": "textDocument/publishDiagnostics",
  "params": {
    "uri": "{{ROOT_URI}}/main.cow",
    "diagnostics": [
      {
        "range": {
          "start": { "line": 1, "character": 0 },
          "end":   { "line": 1, "character": 2 }
        },
        "severity": 1,
        "code": "directive-lookup.unresolved",
        "source": "cowel",
        "message": "No directive with the name \"m\" exists."
      }
    ]
  }
}
```

Use `"diagnostics": []` to assert that a file has no errors.

### Output message ordering

- Within a single `publish_diagnostics_for` call,
  messages are emitted in URI order (sorted).
- `revalidate_includers` appends its messages after the primary validation messages.
- The order in `output.json` must match the actual emission order exactly.

## Diagnostic Format Reference

For unresolved directive names:

| Field | Value |
|---|---|
| `severity` | `1` (error) |
| `code` | `"directive-lookup.unresolved"` |
| `source` | `"cowel"` |
| `message` | `"No directive with the name \"NAME\" exists."` |

**Span rule:** The diagnostic span for a directive invocation
**includes the leading backslash**.
For `\m` at line 1, column 0:
- `start`: `{ "line": 1, "character": 0 }`
- `end`:   `{ "line": 1, "character": 2 }` (1 for `\` + 1 for `m`)

For `\foo` at line 0, column 3:
- `start`: `{ "line": 0, "character": 3 }`
- `end`:   `{ "line": 0, "character": 7 }` (1 for `\` + 3 for `foo`)

## Building and Validating

### Build the LSP WASM target

```bash
cmake --build build --config Debug -j4 --target cowel-lsp-wasm
```

### Run all LSP integration tests (and other Node tests)

```bash
npm test --prefix bindings/node
```

Expected output: all suites pass, including:
```
✔ lsp: include_update_clears_error
```

### Full validation sequence after editing `bindings/lsp/lsp.cpp`

```bash
cmake --build build --config Debug -j4 --target cowel-lsp-wasm
npm test --prefix bindings/node
```

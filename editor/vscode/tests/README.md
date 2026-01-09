# TextMate Grammar Testing

This directory contains tests for the COWEL TextMate grammar using structured JSON assertions.

## Running Tests

Simply run:

```bash
npm run test
```

## Writing Tests

### 1. Create a Test File

Create a `.cowel` file in `tests/fixtures/` with the code you want to test.

```cowel
\h1{Hello World}
```

### 2. Create Assertions

Create a corresponding `.cowel.json` file with your assertions.

```json
{
  "description": "Test heading directive",
  "assertions": [
    {
      "type": "top-scope",
      "line": 0,
      "col": 0,
      "expected": "support.function.directive.cowel",
      "description": "Directive name has correct scope"
    }
  ]
}
```

### 3. Inspecting Scopes

Use the `dump-scopes` utility to see what scopes are actually generated.

```bash
npx ts-node tests/dump-scopes.ts fixtures/your-test.cowel
```

This will show you the full scope chain for each token, which helps you write accurate assertions.

## Assertion Types

All assertions have these common fields:
- `type`: The assertion type (see below)
- `line`: Zero-based line number
- `col`: Zero-based column number
- `description`: Optional human-readable description

### `scope-at`

Asserts that the token at the given position has an exact scope chain.

```json
{
  "type": "scope-at",
  "line": 0,
  "col": 5,
  "expectedScopes": ["source.cowel", "comment.line.cowel"],
  "description": "Inside line comment"
}
```

### `top-scope`

Asserts that the most specific scope at the given position matches.

```json
{
  "type": "top-scope",
  "line": 0,
  "col": 10,
  "expected": "comment.line.cowel",
  "description": "Most specific scope is comment.line"
}
```

### `token-has-scope`

Asserts that the token at the given position contains a specific scope anywhere in its chain.

```json
{
  "type": "token-has-scope",
  "line": 2,
  "col": 0,
  "scope": "comment.block.cowel",
  "description": "Block comment opener has block comment scope"
}
```

### `scope-chain`

Asserts the complete scope chain matches exactly. This is an alias for `scope-at` but with a different field name for clarity.

```json
{
  "type": "scope-chain",
  "line": 2,
  "col": 5,
  "expected": ["source.cowel", "comment.block.cowel"],
  "description": "Inside block comment has proper scope chain"
}
```

### `parent-scope`

Asserts that the second-most-specific scope matches. This is useful for checking the parent context of a token.

```json
{
  "type": "parent-scope",
  "line": 0,
  "col": 3,
  "expected": "meta.content.cowel",
  "description": "Content brace parent scope is meta.content"
}
```

## Test Output

### Passing Tests

```
OK: comments.cowel
```

### Failing Tests

```
FAIL: directives.cowel (1/6 assertions failed)
  Assertion failed: Argument parenthesis has correct scope
    at line 2, column 4
    \p(class="intro"){Welcome}
       ^
    Expected top scope "punctuation.section.arguments.begin.cowel", got "entity.name.tag.cowel"
    Actual scope chain: source.cowel → meta.arguments.cowel → variable.parameter.named.cowel → entity.name.tag.cowel
```

The failure output shows:
- which assertion failed
- the exact position with a marker
- what was expected versus what was found
- the full scope chain for debugging

## Example Test File

See `fixtures/comments.cowel.json` and `fixtures/directives.cowel.json` for complete examples.

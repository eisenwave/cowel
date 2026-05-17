# COWEL Language Summary (Agent Reference)

COWEL is a TeX-like markup language that compiles to HTML.
Source files use `.cow` or `.cowel` extensions.

## The only special character: `\`

At document level, `\` is the **only** character with special meaning.
Everything else — including `<`, `>`, `&`, whitespace, and newlines — is literal text
that gets HTML-escaped and written to output.
This makes COWEL easy to nest foreign content in.

`\` introduces one of:

- **Escape** — `\{`, `\}`, `\\`, `\"` → the literal second character.
  `\+` followed by exactly 4 or 8 hexadecimal digits encodes a Unicode scalar value.
  If 8 digits can be matched, they win over 4.
  Hex letters must be all lowercase or all uppercase.
  `\'NAME'` (apostrophe, uppercase letters/digits/spaces/hyphens, closing apostrophe)
  encodes the Unicode scalar value by character name (e.g. `\'DIGIT ZERO'`, `\'LF'`).
  All escape forms also work **inside string literals** (e.g. `"line1\'LF'line2"`).
  `\` followed by a newline → nothing (joins lines without whitespace).
- **Line comment** — `\: comment text` extends to end of line.
  The trailing newline is consumed, so it leaves no blank line.
- **Block comment** — `\* ... *\` (non-nestable).
- **Directive splice** — `\name(args){content}` (see next section).
- **Expression splice** — `\(expression)` evaluates an expression and inserts its text result.

## Directives: the core abstraction

```
\name(pos1, pos2, named=val){block content}
```

The **group** `(...)` holds arguments; the **block** `{...}` is syntactic sugar for
the last parameter. Either or both may be omitted.

Argument matching follows Kotlin-style rules:
1. `{...}` matches the last parameter.
2. Positional args match parameters left to right.
3. Named args (`name = value`) match by name and may follow positional args,
   but positional args may not follow named args.
4. Unmatched parameters with an optional type receive `null`;
   others with a default receive that default; otherwise an error is raised.

`...` (ellipsis) in a group passes the caller's variadic arguments through unchanged.

## Directives are lazy (macro-like, not function-like)

This is the most surprising semantic.
**Directives have full control over whether and how their arguments are evaluated.**
A directive that ignores its block simply never processes it —
no HTML is generated from the ignored content.
This is not a special case; it is the normal model, like Lisp macros.

```
\cowel_macro(noop){}
\noop{\b{this bold tag is never processed}}  \: produces nothing
```

## Paragraph splitting: blank lines become `<p>`

The top-level document policy automatically wraps text in `<p>` elements,
splitting on blank lines.
This is also how `\cowel_paragraphs{...}` behaves.
A common gotcha: an accidental blank line inside a directive's content
can introduce an unwanted `<p>` split in the output.
Use `\:` (line comment) to eliminate blank lines without adding visible content.

## Content policies

Every piece of content passes through the currently active **content policy**,
which decides how to convert it to HTML.
Policies are piped/chained and ultimately write to the output file.

The top-level policy is **paragraphs → to-HTML**.
The most important policies are:

- **To-HTML** — text is HTML-escaped; directives are processed; raw HTML passes through.
- **Paragraphs** — like to-HTML, but blank lines generate `<p>` splits.
- **Highlight** — like to-HTML, but text gets syntax highlighting applied.
- **No-invoke** — directives are *not* processed; their source is shown literally.
- **Actions** — text, escapes, and comments are ignored; only directives execute.
- **Text-only** — everything collapses to plaintext; HTML from directives is discarded.
- **Source-as-text** — the raw COWEL source of all content becomes plaintext output.

Directives like `\cowel_to_html{...}`, `\cowel_paragraphs{...}`,
`\cowel_highlight(lang){...}`, `\cowel_no_invoke{...}`,
and `\cowel_source_as_text{...}` apply the corresponding policy to their content.

## Types and expressions (scripting context)

Inside a group `(...)`, COWEL switches to a **scripting context** with typed values.

| Type | Notes |
|---|---|
| `unit` | Void result of side-effecting directives. |
| `null` | Error sentinel. `T?` is shorthand for `T \| null`. |
| `bool` | `true` / `false`. |
| `int` | Arbitrary integer. Literals: `123`, `0xff`, `0b101`, `0o17`. |
| `float` | IEEE-754 binary64. Literals: `1.0`, `10e+5`. |
| `str` | UTF-8 string literal, such as `"hello"`. |
| `block` | Lazily evaluated markup snippet. Literal: `{content}`. |
| `group(T...)` | Product/tuple type. Literal: `(a, b)` or `(x=a, y=b)`. |
| `pack T` | Variadic repetition inside a `group`. |
| `named T` | Named member inside a `group`. |
| `lazy T` | Evaluated only when accessed by the directive (deferred). |
| `T \| U` | Union; a value cannot be of union type itself. |

Expressions in groups follow **C-identical precedence and associativity**:
prefix `~ ! + -`, then `* / %`, `+ -`, `< > <= >=`, `== !=`, `&&`, `||`.
All binary and prefix operators desugar to `cowel_*` builtins
(e.g. `a + b` ≡ `cowel_add(a, b)`).
A directive call `name(args)` or `name{block}` is also a valid expression.

Values can be spliced into text using directives and expression splices:
`"x = \(1 + 2)"` or `{x = \(1 + 2)}` evaluates the expression and inserts `3`.
Directive splices work the same way: `"Hello, \name!"`.

A bare **identifier** in an expression that is not followed by a group or block
is an **id-expression** and resolves to the value of the variable with that name.
This is the idiomatic way to read variables:
`myvar` instead of `cowel_var_get("myvar")`.

## Defining macros

```
\cowel_macro(myname){body}
```

Defines a new directive `\myname`.
Inside the body, `\cowel_put` accesses what the caller provided:

- `\cowel_put` — the block content.
- `\cowel_put{0}` — first positional argument (zero-indexed).
- `\cowel_put{argname}` — named argument.
- `\cowel_put(else=fallback){0}` — with fallback if argument is absent.

Multiple names share one body: `\cowel_macro(a, b){...}`.
`\cowel_alias(alias){target}` makes `alias` forward to an existing directive.
`\cowel_actions{...}` runs its content under the actions policy,
which is used to batch macro/alias definitions at the top of a document
without emitting any HTML.

## Essential builtin directives

```
\: HTML generation
\cowel_html_element(div, (id=abc, class=foo)){content}
\cowel_html_self_closing_element(hr, (id=abc))

\: File inclusion
\cowel_include("other.cow")       \: parsed COWEL, inline
\cowel_include_text("file.txt")   \: raw text as str value

\: Special characters
\cowel_char_by_name("EM DASH")    \: exact Unicode name, case-sensitive
\cowel_char_by_entity("amp")      \: HTML entity without & and ;
\cowel_char_by_num(0x2014)        \: by scalar value
\cowel_char_get_num("A")          \: returns int code point of first char
```

## Naming conventions

- Builtin directives: `cowel_snake_case` (guaranteed not to collide with user names).
- User macros/aliases: any identifier of ASCII letters, digits, and `_`.
- `\` is only written at **call sites**; definitions use the bare name.

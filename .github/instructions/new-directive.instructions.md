---
description: "Use when adding a new builtin directive to COWEL: implementing the behavior struct, registering in the directive table, writing semantic tests, adding documentation in new_directives.cow, updating CHANGELOG.md and lang-summary.md."
---

# Adding a New Builtin Directive

Every new `cowel_*` builtin directive requires ALL of the following steps.
Complete them in order; each step depends on the previous ones being in place.

## 1. Declare the behavior struct

**File:** `engine/include/cowel/builtin_directive_set.hpp`

Insert the struct **alphabetically by directive name** relative to adjacent structs.

Choose the right base class based on the return type:

| Return type | Base class | Override method |
|---|---|---|
| `int` | `Int_Directive_Behavior` | `do_evaluate` → `Result<Big_Int, Processing_Status>` |
| `str` (≤56 bytes) | `Short_String_Directive_Behavior` | `do_evaluate` → `Result<Short_String_Value, Processing_Status>` |
| `str` (any length) | `String_Sink_Behavior` | `do_spliced` |
| `bool` | `Bool_Directive_Behavior` | `do_evaluate` → `Result<bool, Processing_Status>` |
| union (`T \| null`) | `Directive_Behavior` directly | `evaluate` → `Result<Value, Processing_Status>` |
| block/content | `Block_Directive_Behavior` | `splice` |

For union return types (e.g. `str | null`, `int | null`), use this exact pattern:

```cpp
struct [[nodiscard]]
Foo_Behavior final : Directive_Behavior {
private:
    static constexpr Type alternatives[] { Type::null, Type::str };
    static constexpr Type return_type = Type::union_of(alternatives);
    static_assert(return_type.is_canonical());

public:
    [[nodiscard]]
    constexpr explicit Foo_Behavior()
        : Directive_Behavior { return_type }
    {
    }

    [[nodiscard]]
    Result<Value, Processing_Status> evaluate(const Invocation&, Context&) const override;
};
```

## 2. Register the directive

**File:** `engine/src/builtin_directive_set.cpp`

Both additions must be in **alphabetical order by directive name**.

Add the constexpr instance near the top of the file:
```cpp
constexpr Foo_Behavior cowel_foo //
    {};
```

Add the table entry in the sorted entries block:
```cpp
COWEL_NAME_AND_BEHAVIOR_ENTRY(cowel_foo),
```

The variable name (`cowel_foo`) is the directive name users call with `\cowel_foo`.

## 3. Implement the behavior

**File:** `engine/src/directives/<category>.cpp`
(Use the existing file matching the directive's category, e.g. `code_point.cpp`,
`str.cpp`. Create a new file only if adding an entirely new category.)

Parameter-matching boilerplate — see surrounding functions in the same file:
```cpp
Result<..., Processing_Status>
Foo_Behavior::evaluate(const Invocation& call, Context& context) const
{
    String_Matcher x_matcher { context.get_transient_memory() };
    Parameter x_parameter { u8"x", Optionality::mandatory, x_matcher };
    Parameter* const parameters[] { &x_parameter };

    const auto args_status = match_call(parameters, call, context);
    if (args_status != Processing_Status::ok) {
        return args_status;
    }
    // ...
}
```

Common diagnostics:
- `diagnostic::type_mismatch` —
    automatically emitted by `match_call` when argument types are wrong.

## 4. Write a semantic test

**File:** `engine/test/files/semantics/<category>/<directive_stem>.cow`

Test files are discovered automatically — no registration needed.
Name the file after the directive name suffix
(e.g. `cowel_char_get_name` → `char/get_name.cow`).

Format:
```cowel
\test_input{
\directive_name("happy path value")

\test_expect_warning("warning.id"){\directive_name("warns on this")}

\test_expect_error("type.mismatch"){\directive_name()}
\test_expect_error("relevant.id"){\directive_name("")}
}

\test_output{
expected output

warned-case output

<error->\directive_name()</error->
<error->\directive_name("")</error->
}
```

Rules:
- **Nested directive calls inside argument `(...)` must omit `\`.**
  Write `\foo(bar("x"))` not `\foo(\bar("x"))`.
  The `\` prefix is only for directive calls in document-text context.
- Always test: the normal happy-path case, `type.mismatch` for missing required
  args, and any directive-specific error and warning cases.
- If the directive can return `null`, test that case; spliced `null` renders as
  the string `"null"` in the output.
- Use `\:` comments to label each case.

## 5. Add documentation

**File:** `docs/new_directives.cow`

Insert a `\cowdoc_dir_h4` section in the correct `\h3` group,
maintaining alphabetical or logical order within the group:

```cowel
\cowdoc_dir_h4("cowel_foo", "Human-readable title")

\pre{
\cowel_highlight_as("markup-tag"){cowel_foo}(\cowdoc_param("x", "str")): \cowel_highlight_as("keyword-type"){str}
}

Prose description of what the directive does.

\Bex{
COWEL markup:
\cowblock{\literally{
\cowel_foo("example")
}}
Generated HTML:
\htmlblock{
\cowel_foo("example")
}
}
```

After editing, **regenerate the golden HTML**:
```bash
./build/cowel-cli run docs/index.cow docs/index.html
```

This updates `docs/index.html`,
which is checked by `Document_Generation.documentation`.
Forgetting this step causes that test to fail.

## 6. Update `CHANGELOG.md`

Under `### Engine` in the current unreleased version block:
```markdown
- Added `cowel_foo` directive for <brief description> (#NNN)
```

`#NNN` is the issue or pull request number for this change.
Do not create issues yourself; request a number from the user.

## 7. Update `.github/lang-summary.md`

If the directive is broadly useful,
add it to the "Essential builtin directives"
code block with a brief inline comment:

```cowel
\cowel_foo("example")     \: what it does
```

Per project policy, `.github/copilot-instructions.md` and
`.github/lang-summary.md` must both stay in sync whenever builtins change.
Update both if the new directive is worth listing,
but this is rarely the case.

## 8. Build and verify

```bash
cmake --build build --config Debug -j4
ctest --test-dir build --output-on-failure
```

All tests must pass, including:
- `Document_Generation.file_tests` — the new semantic test
- `Document_Generation.documentation` — the regenerated golden HTML

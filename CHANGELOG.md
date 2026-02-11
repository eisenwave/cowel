# Changelog

## **Next version** (???)

- change `null` from erroring when spliced to producing the string `"null"`
- add the following string and regular expression directives (#183, #221):
  - `cowel_str_contains`
  - `cowel_str_find`
  - `cowel_str_match`
  - `cowel_str_replace_first`
  - `cowel_str_replace_all`
  - `cowel_str_regex`
- fix `cowel_to_str` for base 36

**Full Changelog**:
[`v0.6.0...master`](https://github.com/eisenwave/cowel/compare/v0.7.0...master)

## **[v0.7.0](https://github.com/eisenwave/cowel/releases/tag/v0.7.0)** (2026-02-05)

- remove support for `-` in directive names (this time for real) (#96)
- remove the following deprecated directives (#98):
  - `Cadd` (use `cowel_add` instead)
  - `Csub` (use `cowel_sub` instead)
  - `Cmul` (use `cowel_mul` instead)
  - `Cdiv` (use `cowel_div_to_zero` instead)
  - `Vget` (use `cowel_var_get` instead)
  - `Vset` (use `cowel_var_let` or `cowel_var_set` instead)
  - `comment` (use `\* ... *\` block comments instead)
  - `literally` (use `cowel_source_as_text` instead)
  - `unprocessed` (use `cowel_no_invoke` instead)
  - `text` (use `cowel_text_only` instead)
- remove most escape sequences, keeping only absolutely necessary ones (#175)
- change the syntax of directive arguments (#87, #107, #109, #111, #112, #121)
  - there are now values/literals of specific types
  - only specific strings are allowed to be unquoted, rather than this being the default
- change `cowel_char_*` directives as follows:
  - `cowel_char_by_entity` takes an `str` and returns a `str`
  - `cowel_char_by_name` takes an `str` and returns a `str`
  - `cowel_char_by_num` takes an `int` and returns a `str`
  - `cowel_char_get_num` takes a `str` and returns an `int`
- change `cowel_include` and `cowel_include_text` as follows:
  - accept a `path: str` parameter instead of a block as input
  - make `cowel_include_text` return `str`
- change the syntax to be parsed in two phases (#175)
- change the `show-number` parameter for heading directives to `show_number`
- add support for typed values in builtin directives (#88)
- add dedicated `\* ... *\` syntax for block comments (#116)
- add `\d("x" = y)` syntax for "computed" argument/member names (#179)
- add the following basic arithmetic directives (#98):
  - `cowel_pos` (`+`)
  - `cowel_neg` (`-`)
  - `cowel_abs`
  - `cowel_sqrt`
  - `cowel_trunc`
  - `cowel_floor`
  - `cowel_ceil`
  - `cowel_nearest`
  - `cowel_nearest_away_zero`
  - `cowel_min`
  - `cowel_max`
  - `cowel_add` (`+`)
  - `cowel_sub` (`-`)
  - `cowel_mul` (`*`)
  - `cowel_div` (`/` for floating-point)
  - `cowel_div_to_zero`
  - `cowel_rem_to_zero`
  - `cowel_div_to_pos_inf`
  - `cowel_rem_to_pos_inf`
  - `cowel_div_to_neg_inf`
  - `cowel_rem_to_neg_inf`
- add the following logical operation directives:
  - `cowel_not` (`!`)
  - `cowel_and` (`&&`)
  - `cowel_or` (`||`)
- add the following comparison directives:
  - `cowel_eq` (`==`)
  - `cowel_ne` (`!=`)
  - `cowel_lt` (`<`)
  - `cowel_gt` (`>`)
  - `cowel_le` (`<=`)
  - `cowel_ge` (`>=`)
- add the following type conversion directives (#125, #128):
  - `cowel_to_str`
  - `cowel_reinterpret_as_float`
  - `cowel_reinterpret_as_int`
- add the following string operation directives (#134, #155):
  - `cowel_str_length`
  - `cowel_str_utf8_length`
  - `cowel_str_to_lower`
  - `cowel_str_to_upper`
- add the following variable management directives (#73):
  - `cowel_var_delete`
  - `cowel_var_exists`
  - `cowel_var_get`
  - `cowel_var_let`
  - `cowel_var_set`
- add a `cowel_as_text` content policy directive (#202)
- add the ability to store groups (and generally not just strings) in variables (#73, #166)
- add support for `abbreviation` and `figment` aliases to `cowel_char_by_name` (#11)
- add language extension for VSCode (#140)
- add support for arbitrary-precision integers (#195)
- fix various memory issues (#131)
- fix some missing code citations for diagnostics in Node.js CLI (#91)
- fix typos in `link rel` (#93)
- fix documentation of insertion block (#92)
- fix crash when too many positional arguments provided (#138)
- also update µlight to a more recent version
  with highlighting support for Python and some other languages
- also optimize/simplify directive name lookup (#102)
- also require at least NodeJS 20.0.0 (#141)

**Full Changelog**:
[`v0.6.0...v0.7.0`](https://github.com/eisenwave/cowel/compare/v0.6.0...v0.7.0)

## **[v0.6.0](https://github.com/eisenwave/cowel/releases/tag/v0.6.0)** (2025-08-23)

- remove support for `-` in directive names
- remove the following deprecated directives:
  - `\html` (use `\cowel_text_as_html` instead)
  - `\htmlblock` (use `\cowel_text_as_html` instead)
  - `\html-*` (use `\cowel_html_element` instead)
  - `\wg21-*` (use `\wg21_*` instead)
  - `\make-*` (use `\make_*` instead)
  - `\lorem-ipsum` (use `\lorem_ipsum` instead)
  - `\c` (use `\cowel_char_by_entity` instead)
  - `\N` (use `\cowel_char_by_name` instead)
  - `\U` (use `\cowel_char_by_num` instead)
  - `\Udigits` (use `\cowel_char_get_num` instead)
  - `\import` (use `\cowel_include` instead)
  - `\include` (use `\cowel_include_text` instead)
  - `\hl` (use `\cowel_highlight_as` instead)
  - `\word` (use `\nobr` instead)
  - `\bug` (use `\Bug` instead)
  - `\decision` (use `\Bdecision` instead)
  - `\delblock` (use `\Bdel` instead)
  - `\diff` (use `\Bdiff` instead)
  - `\example` (use `\Bex` instead)
  - `\indent` (use `\Bindent` instead)
  - `\insblock` (use `\Bins` instead)
  - `\important` (use `\Bimp` instead)
  - `\note` (use `\Bnote` instead)
  - `\tip` (use `\Btip` instead)
  - `\todo` (use `\Btodo` instead)
  - `\warning` (use `\Bwarn` instead)
  - `\macro` (use `\cowel_macro` instead)
  - `\block` (use `\cowel_paragraph_leave` instead)
  - `\inline` (use `\cowel_paragraph_enter` instead)
  - `\paragraphs` (use `\cowel_paragraphs` instead)
  - `\wg21_note` (use `\cowel_macro` instead)
  - `\wg21_example` (use `\cowel_macro` instead)
- remove duplicate checks for (deprecated) `\bib` entries
- remove `prefix` and `suffix` parameters in `\code` (use `\cowel_highlight_phantom` instead)
- change `[` and `]` argument syntax to `(` and `)` (#79)
- change any use of `yes` and `no` arguments to `true` and `false`, respectively
- change warnings for stray/missing arguments to errors in general
- change in the following directives that HTML attributes are provided via `attr` group argument:
  - `\h1`, `\h2`, `\h3`, `\h4`, `\h5`, `\h6`
  - `\cowel_html_element`
- add argument groups (#80)
- add `\-` escape sequence
- add EBNF grammar (`docs/intro/grammar.ebnf`)
- improve consistency for when `"` are applied to HTML attribute values
- fix includes relative to included files not resolving paths correctly (#84)
- also update µlight to enable EBNF syntax highlighting

**Full Changelog**:
[`v0.5.1...v0.6.0`](https://github.com/eisenwave/cowel/compare/v0.5.1...v0.6.0)

## **[v0.5.1](https://github.com/eisenwave/cowel/releases/tag/v0.5.1)** (2025-08-14)

- fix `\cowel_put` not inheriting paragraph,
  preventing paragraph splitting of expanded content

**Full Changelog**:
[`v0.5.0...v0.5.1`](https://github.com/eisenwave/cowel/compare/v0.5.0...v0.5.1)

## **[v0.5.0](https://github.com/eisenwave/cowel/releases/tag/v0.5.0)** (2025-08-12)

- change that trailing empty arguments are not empty positionals (#77)
- deprecate legacy `\macro` directive (#77)
- add ellipsis arguments and argument forwarding (#77)
- add `\cowel_actions` directive (#77)
- fix infinite loop on assertion failure (#75)
- fix assertion failure on empty syntax highlighted content (#75)
- also document `\.` to be a stable escape sequence (#77)

**Full Changelog**:
[`v0.4.0...v0.5.0`](https://github.com/eisenwave/cowel/compare/v0.4.0...v0.5.0)

## **[v0.4.0](https://github.com/eisenwave/cowel/releases/tag/v0.4.0)** (2025-08-09)

- remove deprecated `\item`/`\-item` behavior within `\ul` and `\li` (#68)
- add `\cowel_alias` directive (#65)
- add `\cowel_macro` directive (#68)
- add `\cowel_put` directive (#68)
- add `\cowel_invoke` directive (#52)
- add `-l`/`--severity` option and help menu to native CLI (#60)
- add documentation for npm installation (#57)
- improve performance with better Unicode processing, obsoleting assertions (#61)
- improve performance by buffering HTML snippets during output (#62)
- improve performance of directive name lookup (#66)
- fix trace messages printed by default in native CLI (#60)
- fix potential out-of-bounds bug in string utilities (#72)
- also create CHANGELOG and CONTRIBUTING documents

**Full Changelog**:
[`v0.3.1...v0.4.0`](https://github.com/eisenwave/cowel/compare/v0.3.1...v0.4.0)

## **[v0.3.1](https://github.com/eisenwave/cowel/releases/tag/v0.3.1)** (2025-08-02)

- add Node.js + WASM wrapper as an alternative to native CLI (#53, #54, #56)
- add directives for controlling content policies (`\cowel_to_html` etc.)  (#45)
- add `\cowel_highlight` and `\cowel_highlight_as` directives for syntax highlighting (#48)
- add `\cowel_highlight_phantom` directive (#49)

**Full Changelog**:
[`v0.2.2...v0.3.1`](https://github.com/eisenwave/cowel/compare/v0.2.2...v0.3.1)

## v0.3.0 (2025-08-01)

Never properly published.

## **[v0.2.2](https://github.com/eisenwave/cowel/releases/tag/v0.2.2)** (2025-07-26)

- deprecate legacy `\html-*` directives (#30)
- deprecate hyphens in directive names (#31)
- new `\cowel_html_element` and `\cowel_html_self_closing_element` directives (#28)
- add `\cowel_char_*` directives (#36, #41, #43)
- add `\cowel_paragraphs` and `\cowel_paragraph_*` directives (#38)

**Full Changelog**:
[`v0.2.1...v0.2.2`](https://github.com/eisenwave/cowel/compare/v0.2.1...v0.2.2)

## **[v0.2.1](https://github.com/eisenwave/cowel/releases/tag/v0.2.1)** (2025-07-24)

- fix paragraph splitting following a comment (#17)
- fix wrong results for `\Cdiv` and `\Csub` (#18)
- fix deep paragraph splitting (#19)
- fix undesirable URL-encoding of all `href` attributes (#21)
- fix malformed HTML generated as result of `\style{</style>}` or `\script{</script>}` (#25)

**Full Changelog**:
[`v0.2.0...v0.2.1`](https://github.com/eisenwave/cowel/compare/v0.2.0...v0.2.1)

## **[v0.2.0](https://github.com/eisenwave/cowel/releases/tag/v0.2.0)** (2025-07-24)

- mega refactor (#5, @eisenwave)

**Full changelog**:
[`v0.1.0...v0.2.0`](https://github.com/eisenwave/cowel/compare/v0.1.0...v0.2.0)

## **[v0.1.0](https://github.com/eisenwave/cowel/releases/tag/v0.1.0)** (2025-07-21)

- first tagged release

This build was used for generating C++ proposals
up to the post-Sofia (2025-07) mailing.

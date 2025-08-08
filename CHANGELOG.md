# Changelog

## Next version

- add `\cowel_invoke` directive (#52)
- add `-l`/`--severity` option and help menu to native CLI (#60)
- add documentation for npm installation (#57)
- improve performance with better Unicode processing, obsoleting assertions (#61)
- improve performance by buffering HTML snippets during output (#62)
- fix trace messages printed by default in native CLI (#60)
- also create CHANGELOG and CONTRIBUTING documents

**Full Changelog**:
[`v0.3.1...master`](https://github.com/Eisenwave/cowel/compare/v0.3.1...master)

## **[v0.3.1]** (2025-08-02)

- add Node.js + WASM wrapper as an alternative to native CLI (#53, #54, #56)
- add directives for controlling content policies (`\cowel_to_html` etc.)  (#45)
- add `\cowel_highlight` and `\cowel_highlight_as` directives for syntax highlighting (#48)
- add `\cowel_highlight_phantom` directive (#49)

**Full Changelog**:
[`v0.2.2...v0.3.1`](https://github.com/Eisenwave/cowel/compare/v0.2.2...v0.3.1)

## v0.3.0 (2025-08-01)

Never properly published.

## **[v0.2.2]** (2025-07-26)

- deprecate legacy `\html-*` directives (#30)
- deprecate hyphens in directive names (#31)
- new `\cowel_html_element` and `\cowel_html_self_closing_element` directives (#28)
- add `\cowel_char_*` directives (#36, #41, #43)
- add `\cowel_paragraphs` and `\cowel_paragraph_*` directives (#38)

**Full Changelog**:
[`v0.2.1...v0.2.2`](https://github.com/Eisenwave/cowel/compare/v0.2.1...v0.2.2)

## **[v0.2.1]** (2025-07-24)

- fix paragraph splitting following a comment (#17)
- fix wrong results for `\Cdiv` and `\Csub` (#18)
- fix deep paragraph splitting (#19)
- fix undesirable URL-encoding of all `href` attributes (#21)
- fix malformed HTML generated as result of `\style{</style>}` or `\script{</script>}` (#25)

**Full Changelog**:
[`v0.2.0...v0.2.1`](https://github.com/Eisenwave/cowel/compare/v0.2.0...v0.2.1)

## **[v0.2.0]** (2025-07-24)

- mega refactor (#5, @eisenwave)

**Full changelog**:
[`v0.1.0...v0.2.0`](https://github.com/eisenwave/cowel/compare/v0.1.0...v0.2.0)

## **[v0.1.0]** (2025-07-21)

- first tagged release

This build was used for generating C++ proposals
up to the post-Sofia (2025-07) mailing.

[v0.3.1]: https://github.com/eisenwave/cowel/releases/tag/v0.3.1
[v0.2.2]: https://github.com/eisenwave/cowel/releases/tag/v0.2.2
[v0.2.1]: https://github.com/eisenwave/cowel/releases/tag/v0.2.1
[v0.2.0]: https://github.com/eisenwave/cowel/releases/tag/v0.2.0
[v0.1.0]: https://github.com/eisenwave/cowel/releases/tag/v0.1.0

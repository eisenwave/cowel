# COWEL

COWEL (Compact Web Language) is a markup language
with TeX-like syntax,
intended to produce HTML documents.
See <https://cowel.org/> for language documentation.

This package provides the COWEL processor,
in the form of a small Node.js CLI,
backed by a WASM library which does basically all of the work.

## Usage

To process a COWEL document into an output HTML document:

```sh
cowel INPUT_FILE.cow OUTPUT_FILE.html
```

Use `--help` for more details.

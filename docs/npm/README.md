# COWEL

COWEL (Compact Web Language) is a markup language
with TeX-like syntax,
intended to produce HTML documents.
See <https://cowel.org/> for language documentation.

This package provides the COWEL processor,
in the form of a small Node.js CLI,
backed by a WASM library which does basically all of the work.

## Installation

COWEL is usually installed globally:

```sh
npm i -g cowel
```

**Note**:
If you're on Linux,
you may need to elevate permissions with `sudo` for global installations.

## Usage

After installing,
you can simply run `cowel` or `cowel --help`
to see available commands and options.

To process
a COWEL document located at `input.cow`
into an output HTML file located at `output.html`, use:

```sh
cowel run input.cow output.html
```

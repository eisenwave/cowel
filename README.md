# Missing Middle Markup Language (MMML)

MMML is a markup language with TeX-like syntax,
intended to generate HTML documents,
mainly for proposals and technical text.
Many of its features are purpose-built for use in WG21,
such as for writing C++ proposals.

```tex
\comment{Example:}
Hello, \strong{strong} world!
```
This generates the HTML:
```html
Hello, <strong>strong</strong> world!
```
and usually renders as:

> Hello, **strong** world!

This repository contains documentation of MMML and
tools for processing MMML documents into single `.html` files
(with all dependencies like CSS style packed into the file).

## Motivation

Many similar tools (mpark/wg21, bikeshed, etc.)
are based on Markdown.
This makes them beginner-friendly,
but advanced formatting requires heavy use of Markdown extensions
or mixed use of Markdown and HTML tags.
Metadata such as bibliographies, document information, etc.
also rely on yet another format (e.g. JSON, YAML).

This makes these tools difficult to master and makes the design incoherent.
Why do we need three languages glued together just to format our documents?

MMML is the missing middle, the missing link.
It makes producing HTML a natural part of the language,
lets you specify metadata,
and more, all in one, simple syntax.


## Syntax in a nutshell

MMML has a minimalistic but powerful syntax,
built on top of only three syntactical constructs:
- `\various-directives[maybe, with, arguments = 0]{and content}`
- Plain text.
- Blank lines, separating paragraphs.

A MMML document is a sequence of blocks at the top level,
where a *block* is either delimited by blank lines,
or a *block directive* such as `\codeblock`.
For example:
```tex
Hello, paragraph!
\blockquote{This is a block directive.}
```
Generates the HTML, at a top level:
```html
<p>Hello, paragraph!</p>
<blockquote>This is a block directive.</blockquote>
```

Note that MMML also allows direct HTML generation
using the `\html-*` family of directives.
However, these are only intended as a fallback for when MMML lacks some feature.
Think of them like inline assembly in C or C++, evil low-level stuff.

## Directives

Every directive has three parts:
- `\directive-name`,
- `[comma-separated, list, of, arguments]` (optional)
- `{ block of content }` (optional)

Directive names consist of alphabetic characters, `-`, `_`, and numbers,
but cannot begin with a number.
For example, `\-d` is a directive, but `\0d` expands to "\0d".

These parts have to be connected with no spaces,
meaning that in `\directive {...}`, the `{...}` is literal;
it is not part of the directive.

An empty block is equivalent to no block.
This can be used to connect directives to text.
For example, say we have a `\prefix` directive which expands to "Prefix",
then `\prefix{}suffix` expands to "Prefixsuffix".

An empty argument list is equivalent to no arguments.

### Directive arguments

Directives can be given arguments in two ways:
- Positionally, like `\d[x, z, y]`.
- As named arguments, like `\d[a = x, b = y, c = z]`.

These two ways can also be combined like `\d[x, b = y, z]`.
In the combined case,
any named arguments are matched first, and any leftover arguments
are provided positionally.

For example, `\d[x = y]` "removes" the `x` parameter from positional matching,
so that it cannot be provided another time.
Directive arguments, just like anything else,
can be text or directives.

Note: To provide text that contains a comma as an argument,
you can wrap it in the `\text` directive,
which is a "no-op" in the sense that it expands to the text within.
For example, you can write `\d[x = \text{Text, with, commas.}]`,
which provides `Text, with, commas.` as an argument for `x`.

### Textification

In certain context, formatting is stripped.
We call this *textification*.
For example, the `\html-*` directives transform their arguments into attributes of an HTML tag,
which can only contain plain text, no other HTML tags.

Textification extracts the text contents out of an argument.
For example:
```tex
\html-div[id = \b{bold text}]
```
Expands to:
```html
<div id="bold text"></div>
```

### Brace matching and escape sequences

`\\` is an escape character for `\`, `[`, `]`, `{`, and `}`.
For example, we `\\comment` expands to "\comment", and is not a comment directive.

Inside of any block, brace matching takes place to find the end of that block content.
For example:
```tex
\codeblock{
int main() { }
}
```
The `{` after `main()` increases the brace level,
so that the `}` immediately following it does not end the code block.
`\{` and `\}` produce braces literally, and are ignored for the purpose of finding the end of block content.
For example, `\comment{\{}` is a comment containing an opening brace.

### Directives in arguments

## Whitespace

Whitespace in MMML matters, namely blank lines divide paragraphs.
Furthermore, single line breaks are ignored and turned into word breaks instead,
allowing the MMML source code to remain within a column limit.
For example:
```tex
This
is
one paragraph.

Second paragraph.
```
Renders as:
> This is one paragraph.
>
> Second paragraph.

### `\br` - Line break

When a line break needs to be broken within a paragraph,
this can be accomplished with `\br`,
which generates `<br/>` in HTML.

### `\pagebreak` - Page break

Forces a page break in the specified place.
This is only relevant when printing the HTML document,
and it needs to be divided into pages.

## Comments and Errors

### `\comment{...}`

To insert comments into the source code, you can use the
`\comment{...}` directive.
Potential directives inside of `\comment` are not processed whatsoever,
only brace matching takes place.

Wherever this document says that only certain kinds of directives are permitted,
`\comment` may also be used.

### `\error{...}`

Formatted so that the content inside is visibly erroneous, such as with red text.
Potential directives inside of `\error` are not processed whatsoever,
only brace matching takes place.

Wherever this document says that only certain kinds of directives are permitted,
`\error` may also be used.

Unlike comments, the content of `\error` is still produced.

When a directive fails to be processed (such as due to missing arguments),
it is wrapped in `\error`.
For example, `\def{content}` is missing the name if the directive to be produced,
so it is equivalent to `\error{\def{content}}`.

## Text formatting

MMML allows for basic text formatting using various directives:

| Directive | HTML | Description |
| --------- | ---- | ----------- |
| `\i{...}` | `<i>...</i>` | <i>Italic text</i>
| `\em{...}` | `<em>...</em>` | <em>Emphasized text</em>
| `\b{...}` | `<b>...</b>` | <b>Bold text</b>
| `\strong{...}` | `<strong>...</strong>` | <strong>Strong text</strong>
| `\u{...}` | `<u>...</u>` | <u>Underlined text</u>
| `\ins[color]{...}` | `<ins>...</ins>` | <ins>Inserted text</ins>
| `\s{...}` | `<s>...</s>` | <s>Struck text</s>
| `\del[color]{...}` | `<del>...</del>` | <del>Deleted text</del>
| `\mark{...}` | `<mark>...</mark>` | <mark>Highlighted text</mark>
| `\kbd{...}` | `<kbd>...</kbd>` | <kbd>Keyboard text</kbd>
| `\q{...}` | `<q>...</q>` | <q>Quoted text</q>
| `\small{...}` | `<small>...</small>` | <small>Small text</small>
| `\sub{...}` | `<sub>...</sub>` | <sub>Subscript</sub> text
| `\sup{...}` | `<sup>...</sup>` | <sup>Subscript</sup> text
| `\tt{...}` | `<tt>...</tt>` | <tt>Teletype font</tt>
| `\code{...}`, `\c{...}` | `<code>...</code>` | <code>Source code</code>, possibly syntax-highlighted

Notice that `\i`, `\b`, `\u`, `\s`, and `\tt` are raw formatting directives
without any connotation, and should be avoided.
On the contrary, `\emph`, `\strong`, `\ins`, `\del`, and `\code` specify intent.

While `\i` and `\b` are rendered the same as `\emph` and `\strong` respectively,
`\ins` and `\del` also apply green and red color by default, or another color of choice.

### `\code[lang]{...}`, `\c[lang]{...}` - Inline code

The `\code` (with `\c` as a convenience alias) directive renders text in `code font`
and may apply syntax highlighting.
For example, `\c{x + 1}` renders as `x + 1`.

By default, `\code` uses the most recent code language defined via `\codelang`.
For example:
```tex
\codelang[cpp]

\comment{"int" should be syntax-highlighted as a keyword}
\c{int}

\comment{No syntax-highlighting here:}
\c[plain]{int}
```

The following langauges are supported by default:
<dl>
    <dt>C</dt>
    <dd><code>c</code></dd>
    <dt>C++</dt>
    <dd><code>cpp</code>, <code>cxx</code>, <code>c++</code></dd>
    <dt>Rust</dt>
    <dd><code>rust</code>, <code>rs</code></dd>
    <dt>JavaScript</dt>
    <dd><code>javascript</code>, <code>js</code></dd>
    <dt>TypeScript</dt>
    <dd><code>typescript</code>, <code>ts</code></dd>
</dl>

### `\color[color]{...}` - Text color

The `\color` directive marks a range of text as having a different color.
The specified `color` can be an RGB hex code, or a recognized CSS color name.

## Counters

It is often useful to have increasing numbers in text, such as for paragraph numbers.
The `\count-*` family of directives allows you to manage such counters:

| Directive | Description |
| --------- | ---- |
| `\count-get[id]` | Expands to the current value of the `id` counter, which is initially `1`.
| `\count-set[value, id]` | Sets the `id` counter to `value`, where `value` must be an integer.
| `\count-add[value, id]` | Increases the `id` counter by `value`, where `value` must be an integer.
| `\count-next[value, id]` | Equivalent to `\counter-get[id, value]\counter-add[id, value]`

The `id` is optional.
If none is provided, the default counter is used.
If no `value` is provided in one of the directives, `1` is used by default.

For example, counters can be used for paragraph numbering:
```tex
\count-set
\count-next First paragraph.

\count-next Second paragraph.
```
This renders as follows:

> 1 First paragraph.
>
> 2 Second paragraph.

## Fonts

The defaulf font is a serif font.

### `\font[name]{...}` - Font selection

Displays content using a font family of choice.
Four default families are supported:
- `serif`
- `sans-serif`
- `monospace` (used by `\code`)
- `math` (used by `\math`)

## Various blocks

### `\hr` - Horizontal rule

Expands to a horizontal divider; renders as:

> ---

### `\blockquote` - Blockquote

Blockquotes can be created with `\blockquote`.
These put the text into a block, which indicates that it is a quotation:
```tex
\blockquote{This is a citation.}
```
Renders as:
> This is a citation.

### `\codeblock[lang]{...}` - Code block

Code blocks can be used for one or multiple lines of code, potentially with syntax highlighting.
The contents of a `\codeblock` appear as pre-formatted text in monospace font,
with syntax highlighting applied.
Like for `\code`, the default language can be changed with `\codelang`.

### `\mathblock[lang]{...}` - Math block

Math blocks display math content in a block.
Currently, this merely displays the block content in math font.

### `\pre{...}` - Preformatted block

Like `\codeblock`, but without any syntax highlighting.

### `\samp[lang]{...}` - Sample output

`\samp` (analogous to the `<samp>` HTML element) indicates sample output of a program.
It is similar to a code block, but ignores the default code language specified by `\codelang`.

Sample output is also indicated to be output, rather than some arbitrary block.

### `\block[color]{...}` - A block with special meaning

Creates a block using a primary color, specified by `color`.
This is typically not used directly, but by various helper macros:

| Directive | Description |
| --------- | ---- |
| `\infonote{...}`  | Informative note. Indicates that content provides additional information and can be skipped.
| `\editnote{...}`  | Editor's note. Communicates to the reader why content was written in a specific way.
| `\warning{...}`   | Warning to the reader.
| `\example{...}`   | Example block.
| `\assertion{...}` | Assertion block.
| `\issue{...}`     | An open issue that needs to be resolved.

### `\details{...}` - Details, aka. spoilers

`\details` consist of a `\summary{...}` portion
(which has to be the first directive),
followed by additonal content that is shown on demand.

```tex
\details{
    \summary{Click me}
    Further information about the topic.
}
```
Generates:
<blockquote>
<details>
    <summary>Click me</summary>
    Further information about the topic.
</details>
</blockquote>

The `\summary` directive is optional.


## Lists

### `\ul` - Unordered list

`\ul` defines an unordered list, i.e. a simple list of bullets.
`\ul` shall only contain `\item` directives.

For example:
```tex
\ul{
    \item{Bullet},
    \item{bullet}, and
    \item{bullet!}
}
```
... renders as:
> - Bullet,
> - bullet, and
> - bullet!

### `\ol[sep, start]` - Ordered list

`\ol` defines an ordered list, i.e. a simple list of bullets.
`\ol` shall only contain `\item` directives.

For example:
```tex
\ol{
    \item{First},
    \item{second}, and
    \item{third!}
}
```
... renders as:
> 1. First,
> 2. second, and
> 3. third!

When the `sep` argument is provided and multiple `\ol`s are nested,
their respective counters are joined that separator.
For example:
```tex
\ol{
    \item{
        To be happy means to be neither
        \ol[.]{
            \item{sad, nor}
            \item{miserable.}
        }
    }
}
```
... renders as:
> 1. To be happy means to be neither<br>
>    1.1. sad, nor<br>
>    1.2. miserable

When no `sep` is provided, the counters are not joined.

`start` specifies the initial value of the counter, which is `1` by default.

### `\item[num]{...}` - List items

An item in an ordered or unordered list.

If `num` is provided, it sets the implicit counter of an ordered list.
For example, `\li[3]{item}` is rendered as "3. item" and the next bullet is "4.". 
`num` shall be a number.

### `\dl{...}` - Definition list

A list of terms (`\dt`) and definitions (`\dd`).
For example:
```tex
\dl{
    \dt{Coffee}
    \dd{Black hot drink}
    \dt{Milk}
    \dd{White cold drink}
}
```
Renders as:
<blockquote>
<dl>
    <dt>Coffee</dt>
    <dd>Black hot drink</dd>
    <dt>Milk</dt>
    <dd>White cold drink</dd>
</dl>
</blockquote>

## Tables

### `\table[column-flags...]` - Table

Tables are sequences of rows, which are sequences of cells.
Comma-separated flags can be specified, which control alignment and weight of the columns.

### Column flags

The following column flags are supported:

<dl>
    <dt><code>&lt;</code> (less than)</dt>
    <dd>Align to left (default)</dd>
    <dt><code>&gt;</code> (greater than)</dt>
    <dd>Align to right (default)</dd>
    <dt><code>|</code> (horizontal pipe)</dt>
    <dd>Center horizontically</dd>
    <dt><code>^</code> (caret)</dt>
    <dd>Align vertically to top (default)</dd>
    <dt><code>v</code> (latin character v)</dt>
    <dd>Align vertically to bottom</dd>
    <dt><code>-</code> (hyphen)</dt>
    <dd>Center vertically</dd>
    <dt>Numbers (e.g. <code>123</code>)</dt>
    <dd>Weight of column</dd>
</dl>

The flags `<`, `>`, and `|` contradict each other, and are ignored if more than one is provided for a column.
The same applies to the group `^`, `v`, and `-`.

Column weight specifies how much space is dedicated to the various columns.
For example, with weights `1, 9`, the left column only receives 10% of the space, and the right column receives 90%.

### `\th{...}` - Table heading row

A special row which contains cells that describe the contents of their respective columns.
`\th` shall only contain `\td` directives.

### `\tr{...}` - Table row

A regular row in the table.
`\th` shall only contain `\td` directives.

### `\td[rowspan, colspan]{...}` - Table data

`\td` directives contain the actual table data, i.e. they are the cells of the table.

## File management

### `\include[path]` - File inclusion

Copies the contents of the file at the specified path into the document.
This is a low-level mechanism and is useful for creating MMML
using multiple files, combined into one.

**Convenience macros:**
- `\include-style[path]` -> `\style{\include{\put[path]}}`
- `\include-html[path]` -> `\html{\include{\put[path]}}`
- `\include-script[path]` -> `\script{\include{\put[path]}}`

### `\import[path]` - Document import

Imports a MMML file at the specified path.
The file is processed beforehand, so it needs to be valid MMML in itself,
unlike files copied with `\include`.

**Arguments:**
- `path`: a relative path to the MMML file.

## Headings

MMML lets you define headings with up to six levels:

| Directive | HTML | Description |
| --------- | ---- | ----------- |
| `\h1[id]{...}` | `<h1 id=...>...</h1>` | Document-level heading, usually auto-generated
| `\h2[id]{...}` | `<h2 id=...>...</h2>` | Heading level 2
| `\h3[id]{...}` | `<h3 id=...>...</h3>` | Heading level 3
| `\h4[id]{...}` | `<h4 id=...>...</h4>` | Heading level 4
| `\h5[id]{...}` | `<h5 id=...>...</h5>` | Heading level 5
| `\h6[id]{...}` | `<h6 id=...>...</h6>` | Heading level 6

For headings if any level, an `id` can be specified, which is used for anchor links.
If none is provided, one is auto-generated from title contents.

For example:
```tex
\h2{Some heading}
\h2[anchor]{Another heading}
```
This generates:
```html
<h2 id=some-heading>Some heading</h2>
<h2 id=anchor>Another heading</h2>
```
The reader can then jump to the heading using anchor links such as `mydoc.html#anchor`.

## Macro definitions

MMML supports the definition of macros by the user,
and many directives here are documented in terms of macros.
Macros are defined using `\def`, and can be supplied with attributes and a block of content.

### `\def[identifier, parameter-name...]{...}`

The first argument given to `\def` is is an identifier,
which states the name of the defined macro.
This is followed by zero or more parameters,
followed by a block.

Within this block, `\arg` and `\put` directives expand to the specified argument.

Furthermore, directives such as `\include` are only processed once the macro is used.
You can think of them like functions in a programming language.

### `\arg[arg]`

`\arg` expands the contents of the given argument,
or expands to nothing if none was supplied or the surrounding `\def` has no such argument.

For example, to implement the `\include-html` directive:
```tex
\def[include-html, path]{
    \html{
        \include[\arg[path]]
    }
}
```
`\def[include-html, path]`
defines the `\include-html` directive.
This directive can subsequently be used to `\include` the given file.
Note that `path` can be supplied both positionally and as a named argument.

### `\put`

`\put` expands the contents of the block specified to the macro, or to nothing if there is no block.

Another example, this time using block content:
```tex
\def[red]{\color[#ff0000]{\put}}
...
\red{This text is red.}
```
`\red{This text is red.}` expands to a `<div>` styled with red text.

## HTML Compatibility

### `\html{...}` - Inline HTML

HTML can be generated verbatim.
Directives are still processed within an `\html` block,
so you could e.g. `\include` HTML files into such a block.

For example:
```tex
\html{<div>Content</div>}
```
Generates:
```html
<div>Content</div>
```

### `\style{...}` - Inline CSS

Like `\html`, but for inline CSS.

```tex
\style{
div {
    display: block;
}
}
```
Generates:
```html
<style>
div {
    display: block;
}
</style>
```

### `\script{...}` - Inline JS

Like `\html`, but for inline JS.

```tex
\script{
function awoo() { }
}
```
Generates:
```html
<script>
function awoo() { }
</script>
```

### `\html-*[attributes...]{}` - HTML tag directives

Most HTML features are supported indirectly,
so there's rarely need to use the features in the following section.
For example, you should prefer `\code{int}` over `\html-code{int}`;
the latter has no syntax highlighting and produces a plain HTML element.

Any specified (comma-separated) `attributes` turn into attributes on the HTML tag.
For example:
```tex
\html-div[id=anchor, hidden]{Content}
```
Generates:
```html
<div id=anchor hidden>Content</div>
```

## `\meta{...}` - Document metadata


## References

### `\ref[href]{...}` - References

Raw link to external content, or content within the same document.
Depending on the style of the link, the following options are attempted in order:

- `\ref[https://example.com]` links to another website, and expands to a clickable <https://example.com> text.
- `\ref[#id]` references another section in the document by id.
- `\ref[bib-item]` references an entry in the bibliography.
- `\ref[id]` references an external document under <https://wg21.link/index.json>

## `\bib{...}` - Bibliography

Defines a bibliography.
A bibliography shall only contain `\item` directives.
For example:

```tex
\bib{
    \item[Knuth1997]{
        \title{The Art of Computer Programming}
        \author{\name{Donald Knuth}}
        \publisher{Addison Wesley}
        \date{1997}
        \ref[https://xzy.com/art-of-programming]
    }
}
```

The bibliography needs to be defined prior to `\make-bib` generating the bibliography in the document,
otherwise the generated bibliography will be empty or missing items.

### `\item[id]{...}` - Bibliography items

Every `\item` in a bibliography should have an `id` argument which is used subsequently for `\ref` citations.
Bibliography items shall only contain the following directives:

<dl>
    <dt><code>\title</code></dt>
    <dd>The title of the citation.</dd>
    <dt><code>\publisher{...}</code></dt>
    <dd>Publisher.</dd>
    <dt><code>\author</code></dt>
    <dd>One of the authors. May be used multiple times.</dd>
    <dt><code>\date{...}</code></dt>
    <dd>Date of publication (in no particular format).</dd>
    <dt><code>\ref[...]</code></dt>
    <dd>Specifies a reference.</dd>
</dl>

### `\author{...}`

An `\author` directive stores information about the author.
Its content shall either be plain text, or only contain the following directives:

<dl>
    <dt><code>\name{...}</code></dt>
    <dd>The name of the author.</dd>
    <dt><code>\email{...}</code></dt>
    <dd>An email address. If this is a valid address, will be wrapped in a <code>mailto:</code> link.</dd>
    <dt><code>\affiliation{...}</code></dt>
    <dd>Company or other affiliation.</dd>
</dl>

## Generated content

### `\make-title` - Generate title

Generates a document top-level title (`\h1` and more)
containing the information specified in the `\meta` directive.

### `\make-bib` - Generate bibliography

Generates the bibliography from the information specified in the
`\bib` bibliographies, as well as automatically gathered dependencies from `\ref[id]` such as `\ref[N5001]`.

### `\make-toc[id]` - Generate table of contents

Generates a table of contents based on the headings (`\h1`, `\h2`, etc.)

By default, the document `\h1` is the root,
and the various `\h2` headings form the top level in the table of contents.

If an `id` is specified, the heading with the specified `id` 
is chosen as the root instead.

## WG21 Utilities

In additon to the base directives,
there are certain WG21-specific directives,
which greatly simplify writing proposed wording, among other things.

### `\grammarterm{...}` - Grammatical term

Formats the text inside using a sans-serif, italic font, i.e. in the style of the C++ standard
for grammatical terms.

### `\clause*[ref, num]{}` - (Sub)clause headings

For the purpose of wording citations, C++-style subclause headings are needed.
The directives `\clause1` through `\clause6` specify such a heading.

`num` is the numeric section number.
If specified, it is prepended to the heading.

`ref` is the stable reference to a subclause.
If specified, it is appended to the heading and aligned to the right.

For example:
```tex
\clause2[ref = depr.ellipsis.comma, num = D.5]{Non-comma-separated ellipsis parameters}
Subclause content.
```
Renders as:
> ## D.5 Non-comma-separated ellipsis parameters [depr.ellipsis.comma]
> Subclause content.

### `\clauseref[ref]` - Reference to (sub)clause

References a subclause in the C++ standard,
possibly with an anchor, and wraps this in a link to <https://eel.is/c++draft/>.
For example:
```tex
\clauseref[expr.shift#3.sentence-1]
```
Generates:
```html
<a href="https://eel.is/c++draft/expr.shift#3.sentence-1">
    [expr.shift] paragraph 3, sentence 1
</a>
```
In general, any linked based on <https://eel.is/c++draft/> should be accepted.

### `\wording{...}` - Wording block

Wrapper for C++ wording citations.
This will be rendered indented, so to clearly distinguish editorial instructions
from cited wording.

Typical usage looks as follows:
```tex
Modify \clauseref[conv.prom#8] as follows:

\wording{
    These conversions are called \em{\del{integral} \ins{fun} promotions}.
}
```
This renders as:
> Modify [[conv.prom] paragraph 8](https://eel.is/c++draft/conv.prom#8) as follows:
> <br><br>
> &nbsp;&nbsp;&nbsp;&nbsp;These conversions are called <em><del>integral</del> <ins>fun</ins> promotions</em>.

### `\ins-wording[color]{...}` - Inserted wording block

Like `\wording`, but formats the wording block with color, outlines, or other indicators
so that it's apparent that the entirety of the wording is inserted.
Also affects text color.

This directive should be used when e.g. an entire subclause is inserted into the standard,
or a whole new library entity with various paragraphs describing it, etc.

`color` is an optional color, where the default is green.
This can be used to indicate alternative wording approaches,
or portions of wording that were added in a later revision.

`\del` and `\ins` shall not be used inside of `\ins-wording`.

### `\del-wording[color]{...}` - Deleted wording block

Opposite of `\ins-wording`.
Use this block to surround larger amounts of wording that has been deleted from the standard.

`color` is an optional color, where the default is red.

### `\ins-wording2{...}`, `\del2`, etc. - Secondary insertions and deletions

Convenience macros which use the `ins2` and `del2` colors:

- `\ins2` -> `\ins[ins2]{}`
- `\del2` -> `\del[del2]{}`
- `\ins-wording2{...}` -> `\ins-wording[ins2]{...}`
- `\del-wording2{...}` -> `\del-wording[del2]{...}`

As explained previously, these are useful for indicating optional changes,
changes made in later revisions, etc.

The `ins2` and `del2` colors can be customized using e.g. `\def-color[ins2]`,
but have a predefined value so that these macros are always usable.

### `\wording-editorial[prefix, suffix, num]{...}` - Editorial block in wording

Base directive for editorial blocks within the standard,
such as notes and examples.
For example:
```tex
\wording-editorial[Note, note, 1]{This is a note}.
```
Renders as:
> [ *Note* 1: This is a note. &mdash; *end note* ]

`prefix` and `suffix` are both mandatory,
and are inserted into italicized text at the beginning and end respectively.

`num` is the number of the note.
If omitted, the note has no number.

There are multiple convenience macros:

- `\wording-note[num]{...}` -> `\wording-editorial[Note, note, \arg[num]]{...}`;
    a typical note, indicating text that is not normative
- `\wording-example[num]{...}` -> `\wording-editorial[Example, note, \arg[num]]{...}`;
    an example, also not normative
- `\wording-editnote[num]{...}` -> `\wording-editorial[Editor's note, note, \arg[num]]{...}`;
    an editor's note, neither normative, nor part of any proposed wording. Also given a distinct color.



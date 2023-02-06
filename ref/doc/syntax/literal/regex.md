# Regular Expressions {#regex}

Regular expression (or "regex") literals are denoted by enclosing forward
slashes (`/`) followed immediately by any options.

```riff
/pattern/
```

Riff implements [Perl Compatible Regular Expressions][pcre-home] via the PCRE2
library. The [`pcre2syntax`][pcre-syntax] and [`pcre2pattern`][pcre-pattern] man
pages outline the full syntax and semantics of regular expressions supported by
PCRE2. Riff enables the `PCRE2_DUPNAMES` and `PCRE2_EXTRA_BAD_ESCAPE_IS_LITERAL`
options when compiling regular expressions, which allows duplicated names in
capture groups and ignores invalid or malformed escape sequences, treating them
as literal single characters.

Regular expression literals in Riff also support the same Unicode escape
sequences as string literals (`\uXXXX` or `\UXXXXXXXX`).

Compile-time options are specified as flags immediately following the closing
forward slash. Riff will consume flags until it reaches a non-flag character.
Available options are outlined below.

--------------------------------------------------------------------------------
 Flag  Description
------ -------------------------------------------------------------------------
`A`    Force pattern to become anchored to the start of the search, or the end
       of the most recent match

`D`    `$` matches only the end of the subject string; ignored if `m` is enabled

`J`    Allow names in named capture groups to be duplicated within the same
       pattern (enabled by default)

`U`    Invert the greediness of quantifiers. Quantifiers become ungreedy by
       default, unless followed by a `?`

`i`    Case-insensitive matching

`m`    `^` and `$` match newlines in the subject string

`n`    Disable numbered capturing in parenthesized subpatterns (named ones still
       available)

`s`    Treat the entire subject string as a single line; `.` matches newlines

`u`    Enable Unicode properties for character classes

`x`    Ignore unescaped whitespace in patterns, except inside character classes,
       and allow line comments starting with `#`

`xx`   Same as `x`, but ignore unescaped whitespace inside character classes
--------------------------------------------------------------------------------

: Regular expression modifiers

```riff
/PaTtErN/i      // Caseless matching

// Extended forms - whitespace and line comments ignored

// Equivalent to /abc/
/abc # match "abc"/x

// Equivalent to /add|sub|mul|div/
/ add   # Addition
| sub   # Subtraction
| mul   # Multiplication
| div   # Division
/x
```

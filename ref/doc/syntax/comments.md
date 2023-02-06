# Comments

Riff supports C++-style line comments with `//`, signaling to the interpreter to
ignore everything starting from `//` to the end of the current line. Riff also
supports C-style block comments in the form of `/*...*/`; Riff will ignore
everything following `/*` until it reaches `*/`.

```riff
// This is a comment
/* This is also
   a comment
*/
```

# `num(s[,b])` {#num}

Returns a number interpreted from the string `s` on base (or radix) `b`. If no
base is provided, the default is `0`. When the base is `0`, `num()` will conver
t
to string to a number using the same lexical conventions of the language itself.
`num()` can return an integer or float depending on the string's structure (see
lexical conventions) or if the number is too large to be stored as a signed
64-bit integer.

Valid values for `b` are `0` or integers `2` through `36`. Bases outside this
range will default back to `0`. Providing bases other than `0`, `10` or `16`
will force `s` to only be interpreted as an integer value (current
implementation limitation).

```riff
num('76')           // 76
num('0x54')         // 84
num('54', 16)       // 84
num('0b0110')       // 6
num('0110', 2)      // 6
num('abcxyz', 36)   // 623741435
```

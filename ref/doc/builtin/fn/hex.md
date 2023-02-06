# `hex(x)` {#hex}

Returns a string with the hexadecimal representation of `x` as an integer. The
string is prepended with '`0x`.' Riff currently converts all arguments to
integers.

```riff
hex(123)    // '0x7b'
hex(68.7)   // '0x44'
hex('45')   // '0x2d'
```

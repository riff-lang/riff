# `ord(s)` {#ord}

Given a string `s`, `ord(s)` returns an integer representing the Unicode code
point of the first character in the string.

```riff
ord('A')     // 65
ord('π')     // 960
```

When `s` is a single character string, `ord()` is effectively the inverse of
[`char()`](#char).

When `s` is a string of length greater than 1, `ord(s)` returns an integer
representing the multicharacter sequence where successive bytes are
right-aligned and zero-padded in big-endian form.

```riff
ord('abcd')      // 0x61626364
ord('abcdefgh')  // 0x6162636465666768
ord('\1\2\3\4')  // 0x01020304
```

In the event of overflow, only the lowest 64 bits will remain in the resulting
integer.

```riff
ord('abcdefghi') // 0x6263646566676869 ('a' overflows)
```

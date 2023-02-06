# Numerals

Any string of characters beginning with a number (`0`..`9`) will be interpreted
as a numeric constant. A string of characters will be interpreted as part of a
single numeral until an invalid character is reached. Numerals can be integers
or floating-point numbers in decimal or hexadecimal form. Numbers with the
prefix `0x` or `0X` will be interpreted as hexadecimal. Valid hexadecimal
characters can be any mix of lowercase and uppercase digits `A` through `F`.

```riff
23      // Decimal integer constant
6.7     // Decimal floating-point constant
.5      // Also a decimal floating-point constant (0.5)
9.      // 9.0
0xf     // Hexadecimal integer constant
0XaB    // Valid hexadecimal integer (mixed lowercase and uppercase)
0x.8    // Hexdecimal floating-point constant
```

Riff supports numbers written in exponent notation. For decimal numbers, an
optional decimal exponent part (marked by `e` or `E`) can follow an integer or
the optional fractional part. For hexadecimal numbers, a binary exponent part
can be indicated with `p` or `P`.

```riff
45e2    // 4500
0xffP3  // 2040
0.25e-4 // 0.000025
0X10p+2 // 64
```

Riff supports integers in binary form. Numeric literals with the prefix `0b` or
`0B` will be interpreted as base-2. Riff does not support floating point numbers
with the binary (`0b`) prefix.

```riff
0b1101  // 13 in binary
```

Additionally, Riff supports arbitrary underscores in numeric literals. Any
number of underscores can appear between digits.

Some valid examples:

```riff
1_2
12_
1_2_
1__2_
300_000_000
0x__80
45_e2
0b1101_0011_1010_1111
```

Some *invalid* examples:

```riff
_12     // Will be parsed as an indentifier
0_x80   // Underscore cannot be between `0` and `x`
```

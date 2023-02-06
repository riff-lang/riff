# Strings

String literals are denoted by matching enclosing single quotation (`'`) or
double quotation marks (`"`). String literals spanning multiple lines will have
the newline characters included. Alternatively, a single backslash (`\`) can be
used in a string literal to indicate that the following newline be ignored.

```riff
'Hello, world!'

'String spanning
multiple
lines'

'String spanning \
multiple lines \
without newlines'
```

Riff supports the use of the backslash character (`\`) to denote C-style escape
sequences.

| Character | ASCII code (hex) | Description       |
| :-------: | :--------------: | -----------       |
| `a`       | `07`             | Bell              |
| `b`       | `08`             | Backspace         |
| `e`       | `1B`             | Escape            |
| `f`       | `0C`             | Form feed         |
| `n`       | `0A`             | Newline/Line feed |
| `r`       | `0D`             | Carriage return   |
| `t`       | `09`             | Horizontal tab    |
| `v`       | `0B`             | Vertical tab      |
| `'`       | `27`             | Single quote      |
| `\`       | `5C`             | Backslash         |

Riff also supports arbitrary escape sequences in decimal and hexadecimal forms.

| Sequence | Description                                                   |
| :-       | :---------                                                    |
| `\nnn`   | Octal escape sequence with up to three octal digits           |
| `\xnn`   | Hexadecimal escape sequence with up to two hexadecimal digits |

: Decimal/hexadecimal escape sequence formats

Escaped [Unicode][wiki-unicode] literals are supported in the following forms.

| Sequence     | Description                                             |
| :-------     | -----------                                             |
| `\uXXXX`     | Unicode escape sequence with up to 4 hexadecimal digits |
| `\UXXXXXXXX` | Unicode escape sequence with up to 8 hexadecimal digits |

: Unicode escape sequence formats

```riff
'\u3c0'     // 'π'
'\U1d11e'   // '𝄞'
```

Riff also supports interpolation of variables/expressions in string literals
(aka [string interpolation][wiki-string-interpolation]). Expressions can be delimited by either braces
(`{}`) or parentheses (`()`). The full expression grammar is supported within an
interpolated expression.

```riff
x = 'world'
str = 'Hello #x!'   // 'Hello, world!'

sum = '#{1+2} == 3'
mul = 'square root of 4 is #(sqrt(4))'
```

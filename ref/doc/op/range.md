# Ranges {#ranges}

The `..` operator defines an integral range, which is a subtype in Riff. Ranges
can contain an optional interval, denoted by an expression following a colon
(`:`). Operands can be left blank to denote the absence of a bound, which will
be interpreted differently based on the operation. There are 8 total
permutations of valid ranges in Riff.

| Syntax   | Range                              |
| :----:   | -----                              |
| `x..y`   | $[x..y]$                           |
| `x..`    | $[x..$`INT_MAX`$]$                 |
| `..y`    | $[0..y]$                           |
| `..`     | $[0..$`INT_MAX`$]$                 |
| `x..y:z` | $[x..y]$ on interval $z$           |
| `x..:z`  | $[x..$`INT_MAX`$]$ on interval $z$ |
| `..y:z`  | $[0..y]$ on interval $z$           |
| `..:z`   | $[0..$`INT_MAX`$]$ on interval $z$ |

All ranges are inclusive. For example, the range `1..7` will include both `1`
and `7`. Riff also infers the direction of the range if no `z` value is
provided.

Ranges can be used in [`for`] loops to iterate over a range of numbers.

Ranges can also extract arbitrary substrings when used in a
[subscript](#subscripting) expression with a string. When subscripting a string
with a range such as `x..`, Riff will truncate the range to the end of the
string to return the string's suffix starting at index `x`.

```riff
hello = 'Helloworld'
hello[5..]              // 'world'
hello[..4]              // 'Hello'
hello[..]               // 'Helloworld'
```

Specifying an interval $n$ allows you to extract a substring with every $n$
characters.

```riff
abc = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
abc[..:2]               // 'ACEGIKMOQSUWY'
```

Reversed strings can be easily extracted with a downward range.

```riff
a = 'forwardstring'
a[#a-1..0]              // 'gnirtsdrawrof'
```

As mentioned in the [overview], a range is a type of Riff value. This means
ranges can be stored in variables as well as passed as function parameters and
returned from function calls.

# Overview

Riff is dynamically-typed. Identifiers/variables do not contain explicit type
information and the language has no syntactic constructs for specifying types.
Values, however, are implicitly typed; carrying their own type information.

All Riff values are first-class, meaning values of any type can be stored in
variables, passed around as function arguments or returned as results from
function calls.

Internally, a Riff value can be any of the following types:

- `null`
- Integer
- Float
- String
- Regular expression
- Range
- Table
- File handle
- Riff function (user-defined)
- C function (built-in functions)

`null` is a special value in Riff, typically representing the absence of a
value. `null` is different than `0`, `0.0` or the empty string (`''`).

Numbers in Riff can be integers or floats. Integers in Riff are signed 64-bit by
default (`int64_t` in C). Floats in Riff are identical to a C `double` by
default. Integer to float conversion (and vice versa) is performed implicitly
depending on the operation and is designed to be completely transparent to the
user.

Strings in Riff are immutable sequences of 8-bit character literals.

[Regular expressions][wiki-regex] in Riff define patterns which are used for
performing various string-searching operations.

Ranges are a special "subtype" in Riff that allow the user to define a range of
integral values with an optional specified interval. Ranges can be used in [for]
loops to iterate through a sequence of numbers or in string subscripting to
easily extract different types of substrings.

Tables are the single compound data structure available in Riff. Table elements
can be any type of Riff value. Storing `null` as a table element effectively
deletes that key/value pair.

Tables in Riff are [associative arrays][wiki-assoc-array]. Any type of Riff
value (even `null`) is a valid key for a given table element.

User-defined and built-in functions are treated just as any other value.

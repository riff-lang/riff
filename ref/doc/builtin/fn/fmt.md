# `fmt(f, ...)` {#fmt}

Returns a formatted string of the arguments following the format string `f`.
This functions largely resembles the C function `sprintf()` without support for
length modifiers such as `l` or `ll`.

Each conversion specification in the format string begins with a `%` character
and ends with a character which determines the conversion of the argument. Each
specification may also contain one or more flags following the initial `%`
character.

--------------------------------------------------------------------------------
 Flag    Description
-------- -----------------------------------------------------------------------
`0`      For numeric conversions, leading zeros are used to pad the string
         instead of spaces, which is the default.

`+`      The sign is prepended to the resulting conversion. This only applies to
         signed conversions (`d`, `f`, `g`, `i`).

*space*  If the result of a signed conversion is non-negative, a space is
         prepended to the conversion. This flag is ignored if `+` is specified.

`-`      The resulting conversion is left-justified instead of right-justified,
         which is the default.
--------------------------------------------------------------------------------

: Format modifiers

A *minimum field width* can be specified following any flags (or `%` if no flags
specified), provided as an integer value. The resulting conversion is padded
with spaces on to the left by default, or to the right if left-justified. A `*`
can also be specified in lieu of an integer, where an argument will be consumed
(as an integer) and used to specify the minimum field width.

The *precision* of the conversion can be specified with `.` and an integer value
or `*`, similar to the minimum field width specifier. For numeric conversion,
the precision specifies the minimum number of digits for the resulting
conversion. For strings, it specifies the maximum number of characters in the
conversion. Precision is ignored for character conversions (`%c`).

The table below outlines the available conversion specifiers.

--------------------------------------------------------------------------------
 Specifier  Description
----------- --------------------------------------------------------------------
`%`         A literal `%`

`a` / `A`   A number in hexadecimal exponent notation (lowercase/uppercase).

`b`         An *unsigned* binary integer

`c`         A single character.

`d` / `i`   A *signed* decimal integer.

`e` / `E`   A number in decimal exponent notation (lowercase `e`/uppercase `E`
            used).

`f` / `F`   A decimal floating-point number.

`g` / `G`   A decimal floating-point number, either in standard form (`f`/`F`)
            or exponent notation (`e`/`E`); whichever is shorter.

`m`         A multi-character string (from an integer, similar to `%c`).

`o`         An *unsigned* octal integer.

`s`         A character string.

`x` / `X`   An *unsigned* hexadecimal integer (lowercase/uppercase).
--------------------------------------------------------------------------------

: Format conversion specifiers

Note that the `%s` format specifier will try to handle arguments of any type,
falling back to `%d` for integers and `%g` for floats.

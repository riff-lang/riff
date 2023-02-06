# Subscripting

The `[]` operator is used to subscript a Riff value. All Riff values can be
subscripted except for functions. Subscripting any value with an out-of-bounds
index will evaluate to `null`.

Subscripting a numeric value with expression $i$ will retrieve the
$i$<sup>th</sup> character of that number as if it were a string in its base-10
form (index starting at `0`).

```riff
34[0]       // '3'
0.12[1]     // '.'
(-45)[0]    // '-'
```

Subscripting a string with expression $i$ retrieves the character at index $i$,
as if the string were a contiguous table of characters.

```riff
'Hello'[1]  // 'e'
```

Note that any subscripting or indexing into string values will only be treated
as if the characters in the string were byte-sized. I.e. You cannot arbitrarily
subscript a string value with an integer value and extract a substring
containing a Unicode character larger than one byte.

Naturally, subscripting a table with expression $i$ will perform a table lookup
with the key $i$.

```riff
pedals = ['Fuzz', 'Wah-Wah', 'Uni-Vibe']
pedals[0]   // 'Fuzz'
```

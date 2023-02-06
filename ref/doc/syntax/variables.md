# Variables

```ebnf
id = /[A-Za-z_][A-Za-z0-9_]*/
```
A variable represents a place to store a value in a Riff program. Variables can
be global or local in scope.

A valid identifier is a string of characters beginning with a lowercase letter
(`a`..`z`), uppercase letter (`A`..`Z`) or underscore (`_`). Numeric characters
(`0`..`9`) are valid in identifiers, but not as a starting character.

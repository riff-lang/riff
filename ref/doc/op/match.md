# Pattern Matching

| Operator | Type  | Description   |
| :------: | ----  | -----------   |
| `~`      | Infix | Match         |
| `!~`     | Infix | Negated match |

Pattern match operations can be performed using the infix matching operators.
The left-hand side of the expression is the *subject* and always treated as a
string. The right-hand side is the *pattern* and always treated as a regular
expression.

The result of a standard match (`~`) is `1` is the subject matches the pattern
and `0` if it doesn't. The negated match (`!~`) returns the inverse.

```riff
'abcd'  ~ /a/   // 1
'abcd' !~ /a/   // 0
```

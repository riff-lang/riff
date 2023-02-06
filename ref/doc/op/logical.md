# Logical Operators

| Operator   | Type   | Description   |
| :------:   | ----   | -----------   |
| `!` `not`  | Prefix | Logical `NOT` |
| `&&` `and` | Infix  | Logical `AND` |
| `||` `or`  | Infix  | Logical `OR`  |

The operators `||` and `&&` are [short-circuiting][wiki-short-circuit]. For example, in the
expression `lhs && rhs`, `rhs` is evaluated only if `lhs` is "truthy." Likewise,
in the expression `lhs || rhs`, `rhs` is evaluated only if `lhs` is *not*
"truthy."

Values which evaluate as "false" are `null`, `0` and the empty string (`''`).

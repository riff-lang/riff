# Ternary Conditional Operator

The `?:` operator performs similarly to other C-style languages.

*condition* `?` *expr-if-true* `:` *expr-if-false*

The expression in between `?` and `:` in the ternary conditional operator is
treated as if parenthesized. You can also omit the middle expression entirely.

```riff
x ?: y  // Equivalent to x ? x : y
```

Note that if the middle expression is omitted, the leftmost expression is only
evaluated once.

```riff
x = 1
a = x++ ?: y    // a = 1; x = 2
```

# `until`

```ebnf
until_stmt = 'until' expr stmt
           | 'until' expr '{' stmt_list '}'
```

See [`while`] statements. `until` is exactly like `while`, except control
transfers outside the enclosing loop if the expression *expr* to true.

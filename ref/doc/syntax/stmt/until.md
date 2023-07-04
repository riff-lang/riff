# `until`

```ebnf
until_stmt = 'until' expr stmt
           | 'until' expr '{' stmt_list '}'
```

`until` is exactly like [`while`], except control transfers outside the
enclosing loop if the expression *expr* evaluates to true.

# `while`

```ebnf
while_stmt = 'while' expr stmt
           | 'while' expr '{' stmt_list '}'
```

A `while` statement declares a simple loop structure where the statement(s)
following the expression *expr* are repeatedly executed until *expr* evaluates
to `0`.

Like all loop structures in Riff, the statement(s) inside a loop body establish
their own local scope. Any locals declared inside the loop body are not
accessible outside of the loop body. The expression following `while` has no
access to any locals declared inside the loop body.

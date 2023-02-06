# `do`

```ebnf
do_stmt = 'do' stmt 'until' expr
        | 'do' stmt 'while' expr
        | 'do' '{' stmt_list '}' 'until' expr
        | 'do' '{' stmt_list '}' 'while' expr
```

A `do` statement declares a *do-until* or *do-while* loop structure, which
repeatedly executes the statement or brace-enclosed list of statements until the
expression following the `while` keywords evaluates to `0` (or `1` following
`until`).

Like all loop structures in Riff, the statement(s) inside a loop body establish
their own local scope. Any locals declared inside the loop body are not
accessible outside of the loop body. The `until`/`while` expression is
considered to be outside the loop body.

A `do` statement declared without an `until`/`while` condition is invalid and
will cause an error to be thrown upon compilation.

# `loop`

```ebnf
loop_stmt = 'loop' stmt
          | 'loop' '{' stmt_list '}'
```

A `loop` statement declares an *unconditional* loop structure, where
statement(s) inside the body of the loop are executed repeatedly. This is in
contrast to *conditional* loop structures in Riff, such as `do`, `for` or
`while`, where some condition is evaluated before each iteration of the loop.

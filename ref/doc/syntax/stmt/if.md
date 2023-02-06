# `if`

```ebnf
if_stmt = 'if' expr stmt {'elif' expr ...} ['else' ...]
        | 'if' expr '{' stmt_list '}' {'elif' expr ...} ['else' ...]
```

An `if` statement conditionally executes code based on the result of `expr`. If
the `expr` evaluates to non-zero or non-`null`, the succeeding statement or list
of statements is executed. Otherwise, the code is skipped.

If an `else` statement is provided following an `if` statement, the code in the
`else` block is only executed if the `if` condition evaluated to zero or `null`.
An `else` statement always associates to the closest preceding `if` statement.

Any statements between an `if` and `elif` or `else` statements is invalid; Riff
will throw an error when compiling an `else` statement not attached to an `if`
or `elif`.

`elif` is syntactic sugar for `else if`. Riff allows either syntax in a given
`if` construct.

```riff
// `elif` and `else if` used in the same `if` construct
x = 2
if x == 1 {
  ...
} elif x == 2 {
  ...
} else if x == 3 {
  ...
} else {
  ...
}
```

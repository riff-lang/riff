# `return`

```ebnf
ret_stmt = 'return' [expr]
```

A `return` statement is used for returning control from a function with an
optional value.

The empty `return` statement highlights a pitfall with Riff's grammar. Consider
the following example.

```riff
if x == 1
  return
x++
```

At first glance, this code indicates to return control with no value if `x`
equals `1` or increment `x` and continue execution. However, when Riff parses
the stream of tokens above, it will consume the expression `x++` as part of the
`return` statement. This type of pitfall can be avoided by appending a semicolon
(`;`) to `return` or enclosing the statement(s) following the `if` conditional
in braces.

```riff
if x == 1
  return;
x++
```

```riff
if x == 1 {
  return
}
x++
```

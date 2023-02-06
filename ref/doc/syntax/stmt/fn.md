# `fn`

```ebnf
fn_stmt = 'fn' id ['(' [id {',' id}] ')'] '{' stmt_list '}'
```

A function statement declares the definition of a *named* function. This is in
contrast to an *anonymous* function, which is parsed as part of an [expression
statement].

```riff
fn f(x) {
  return x ** 2
}

fn g() {
  return 23.4
}

// Parentheses not required for functions without parameters
fn h {
  return 'Hello'
}
```

More information on user-defined functions in Riff can be found in the
[functions] section.

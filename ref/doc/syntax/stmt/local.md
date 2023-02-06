# `local`

```ebnf
local_stmt = 'local' expr {',' expr}
           | 'local' fn_stmt
```

`local` declares a variable visible only to the current block and any descending
code blocks. Multiple variables can be declared as `local` with a
comma-delimited expression list, similar to expression lists in expression
statements.

A local variable can reference a variable in an outer scope of the same name
without altering the outer variable.

```riff
a = 25
if 1 {
  local a = a     // Newly declared local `a` will be 25
  a += 5
  print(a)        // Prints 30
}
print(a)          // Prints 25
```

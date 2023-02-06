# `for`

```ebnf
for_stmt = 'for' id [',' id] 'in' expr stmt
         | 'for' id [',' id] 'in' expr '{' stmt_list '}'
```

A `for` statement declares a generic loop structure which iterates over the
item(s) in the `expr` result value. There are two general forms of a `for` loop
declaration:

- `for v in s {...}`
- `for k,v in s {...}`

In the first form, the value `s` is iterated over. Before each iteration, the
variable `v` is populated with the *value* of the next item in the set.

In the second form, the value `s` is iterated over. Before each iteration, the
variable `k` is populated with the *key*, while variable `v` is populated with
the *value* of the next item in a set.

In both forms, the variables `k` and `v` are local to the inner loop body. Their
values cannot be accessed once the loop terminates.

```riff
table = ['foo', 'bar', 'baz']

// This iterates over each item in `table`, populating `k` with the current
// table index, and `v` with the corresponding table element
for k,v in table {
  // First iteration:  k = 0, v = 'foo'
  // Second iteration: k = 1, v = 'bar'
  // Third iteration:  k = 2, v = 'baz'
}
```

Note that the value to be iterated over is evaluated exactly *once*. A copy of
the value is made upon initialization of a given iterator. This avoids an issue
where a user continually adds items to a given set, effectively causing an
infinite loop.

The order in which tables are iterated over is not *guaranteed* to be in-order
for integer keys due to the nature of the table implementation. However, in
most cases, tables will be traversed in order for integer keys $0..n$ where $n$
is the last element in a contiguous table. If a table is constructed using the
constructor syntax, it is guaranteed to be traversed in-order, so long as no
other keys were added. Even if keys were added, tables are typically traversed
in-order. Note that negative indices will always come after integer keys
$\geqslant 0$.

The value to be iterated over can be any Riff value, except functions. For
example, iterating over an integer `n` will populate the provided variable with
the numbers $[0..n]$ (inclusive of `n`). `n` can be negative.

```riff
// Equivalent to `for (i = 0; i <= 10; ++i)`
for i in 10 {
  // ...
}

// Equivalent to `for (i = 0; i >= -10; --i)`
for i in -10 {
  // ...
}
```

Iterating over an integer `n` while using the `k,v` syntax will populate `v`
with $[0..n]$, while leaving `k` as `null`.

Currently, floating-point numbers are truncated to integers when used as the
expression to iterate over.

Iterating over a string is similar to iterating over a table.

```riff
for k,v in 'Hello' {
  // k = 0, v = 'H'
  // k = 1, v = 'e'
  // ...
  // k = 4, v = 'o'
}
```

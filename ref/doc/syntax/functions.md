# Functions

```ebnf
fn_stmt = 'fn' id ['(' [id {',' id} ')'] '{' stmt_list '}'
```

There are two basic "forms" of defining functions in Riff. The first is defining
a "named" function, which populates either the global or local namespace with
the function.

```riff
fn f(x) {
  return x + 1
}

local fn g(x) {
  return x - 1
}
```

The second is [anonymous functions][wiki-anon-fn], which are parsed as part of
an [expression statement](#expression-statements).

```riff
f = fn(x) {
  return x + 1
}

local g = fn(x) {
  return x - 1
}
```

A key difference between the two forms is that named functions can reference
themselves [recursively][wiki-recursion], whereas anonymous functions cannot.

Riff allows all functions to be called with fewer arguments, or more arguments
than the specified arity of a given function. The virtual machine will
compensate by passing `null` for any insufficient arguments, or by discarding
extraneous arguments. Note that this is not true [variadic
function][wiki-variadic-fn] support.

```riff
// Arity of the function is 3
fn f(x, y, z) {
  ...
}

f(1,2,3)    // x = 1        y = 2       z = 3
f(1,2)      // x = 1        y = 2       z = null
f(1,2,3,4)  // x = 1        y = 2       z = 3       (4 is discarded)
f()         // x = null     y = null    z = null
```

Additionally, many included library functions are designed to accept a varying
number of arguments, such as [`atan()`](#atan) and [`fmt()`](#fmt).

## Scoping

Currently, functions only have access to global variables and their own
parameters and local variables. Functions cannot access any local variables
defined outside of their scope, even if a `local` is defined at the higher scope
than the function.

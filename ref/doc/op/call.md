# Function Calls

`()` is a postfix operator used to execute function calls. Arguments are passed
as a comma-delimited list of expressions inside the parentheses.

```riff
fn max(x,y) {
  return x > y ? x : y
}

max(1+4, 3*2)
```

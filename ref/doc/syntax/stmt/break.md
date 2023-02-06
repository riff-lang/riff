# `break`

`break` is a control-flow construct which will immediately exit the current loop
when reached. `break` is invalid outside of a loop structure; `riff` will throw
an error when trying to compile a `break` statement outside of a loop.

```riff
while 1 {
  print('This will print')
  break
  print('This will not print')
}
// program control transfers here
```

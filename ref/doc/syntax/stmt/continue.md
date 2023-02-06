# `continue`

A `continue` statement causes the program to skip the remaining portion of the
current loop, jumping to the end of the of the loop body. Like [`break`],
`continue` is invalid outside of a loop structure; `riff` will throw an error
when trying to compile a `continue` statement outside of a loop.

```riff
do {
  // ...
  continue
  // ...
  // `continue` jumps here
} while 1

for x in y {
  // ...
  continue
  // ...
  // `continue` jumps here
}

while 1 {
  // ...
  continue
  // ...
  // `continue` jumps here
}
```

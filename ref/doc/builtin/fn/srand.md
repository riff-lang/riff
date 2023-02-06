# `srand([x])` {#srand}

Initializes the PRNG with seed `x`. If `x` is not given, `time(0)` is used. When
the PRNG is initialized with a seed `x`, `rand()` will always produce the same
sequence of numbers.

*The following is only an example and may not accurately reflect the expected
output for any particular version of Riff.*

```riff
srand(3)    // Initialize PRNG with seed '3'
rand()      // 0.783235
rand()      // 0.863673
```

`srand()` returns the value used to seed the PRNG.

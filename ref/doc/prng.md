# Pseudo-Random Numbers {#prng}

Riff implements the [`xoshiro256**`][xoshiro] generator to produce pseudo-random
numbers. When the virtual machine registers the built-in functions, the PRNG is
initialized once with [`time(0)`][cppref-time]. Riff provides an
[`srand()`](#srand) function documented below to allow control over the sequence
of the generated pseudo-random numbers.

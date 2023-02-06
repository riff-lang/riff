# `rand([m[,n]])` {#rand}

| Syntax            | Type    | Range                        |
| :-----            | ----    | -----                        |
| `rand()`          | Float   | $[0,1)$                      |
| `rand(0)`         | Integer | $[$`INT_MIN`$..$`INT_MAX`$]$ |
| `rand(n)`         | Integer | $[0 .. n]$                   |
| `rand(m,n)`       | Integer | $[m .. n]$                   |
| `rand(`*range*`)` | Integer | See [ranges]                 |

When called without arguments, `rand()` returns a pseudo-random floating-point
number in the range $[0,1)$. When called with `0`, `rand(0)` returns a
pseudo-random Riff integer (signed 64-bit). When called with an integer `n`,
`rand(n)` returns a pseudo-random Riff integer in the range $[0 .. n]$. `n` can be
negative. When called with 2 arguments `m` and `n`, `rand(m,n)` returns a
pseudo-random integer in the range $[m .. n]$. `m` can be greater than `n`.

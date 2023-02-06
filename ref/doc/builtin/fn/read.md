# `read([a[,b]])` {#read}

Reads data from a file stream, returning the data as a string if successful.

--------------------------------------------------------------------------------
Syntax        Description
------------- ------------------------------------------------------------------
`read([f])`   Read a line from file `f`

`read([f,]m)` Read input from file `f` according to the mode specified by `m`

`read([f,]n)` Read at most `n` bytes from file `f`

`read([f,]0)` Returns `0` if [end-of-file][wiki-eof] has been reached in file
              `f`; `1` otherwise
--------------------------------------------------------------------------------

When a file `f` is not provided, `read()` will operate on `stdin`. The default
behavior is to read a single line from `stdin`. Providing a mode string allows
control over the read operation. Providing an numeric value `n` specifies that
`read()` should read up to `n` bytes from the file. `read([f,]0)` is a special
case to check if the file still has data left to be read.

| Mode      | Description                           |
| ----      | -----------                           |
| `a` / `A` | Read until [EOF][wiki-eof] is reached |
| `l` / `L` | Read a line                           |

: read() modes

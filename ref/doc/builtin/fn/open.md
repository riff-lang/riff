# `open(s[,m])` {#open}

Opens the file indicated by the file name `s` in the mode specified by the
string `m`, returning the resulting file handle.

| Flag | Mode       |
| ---- | ----       |
| `r`  | Read       |
| `w`  | Write      |
| `a`  | Append     |
| `r+` | Read/write |
| `w+` | Read/write |
| `a+` | Read/write |

The flag `b` can also be used to specify binary files on non-POSIX systems.

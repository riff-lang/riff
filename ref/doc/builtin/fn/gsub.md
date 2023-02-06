# `gsub(s,p[,r])` {#gsub}

Returns a copy of `s`, as a string, where all occurrences of `p`, treated as a
regular expression, are replaced with `r`. If `r` is not provided, all
occurrences of `p` will be stripped from the return string.

If `r` is a string, dollar signs (`$`) are parsed as escape characters which can
specify the insertion of substrings from capture groups in `p`.

--------------------------------------------------------------------------------
 Format        Description
-------------- -----------------------------------------------------------------
`$$`           Insert a literal dollar sign character (`$`)

`$x`           Insert a substring from capture group `x` (Either name or number)
<br>`${x}`

`$*MARK`       Insert a control verb name
<br>`${*MARK}`
--------------------------------------------------------------------------------

```riff
// Simple find/replace
gsub('foo bar', /bar/, 'baz')   // 'foo baz'

// Strip whitespace from a string
gsub('a b c d', /\s/)           // 'abcd'

// Find/replace with captures
gsub('riff', /(\w+)/, '$1 $1')  // 'riff riff'
```

# Field Table Operator

`$` is a special prefix operator used for accessing the field table. `$` has
the highest precedence of all Riff operators and can be used to read from or
write to field table.

The field table is used to access substrings resulting from pattern matches and
captured subexpressions in regular expressions. When a match is found, it is
stored as a string in `$0`. Each subsequent capture group is stored in `$n`,
starting from `1`.

```riff
// $1 = 'fish'
if 'one fish two fish' ~ /(fish)/
  print('red', $1, 'blue', $1)

// $1 = 'foo'
// $2 = 'bar'
gsub('foo bar', /(\w+) (\w+)/, '$2 $1') // 'bar foo'
```

Currently, Riff does not purge the field table upon each regex operation. Old
captures will be only ever be overwritten by new ones.

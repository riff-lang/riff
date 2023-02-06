# `split(s[,d])` {#split}

Returns a table with elements being string `s` split on delimiter `d`, treated
as a regular expression. If no delimiter is provided, the regular expression
`/\s+/` (whitespace) is used. If the delimiter is the empty string (`''`), the
string is split into a table of single-byte strings.

```riff
// Default behavior, split on whitespace
sentence = split('A quick brown fox')

// Print the words on separate lines in order
for word in sentence {
  print(word)
}

// Split string on regex delimiter
words = split('foo1bar2baz', /\d/)
words[0]        // 'foo'
words[1]        // 'bar'
words[2]        // 'baz'

// Split string into single-byte strings
chars = split('Thiswillbesplitintochars','')
chars[0]        // 'T'
chars[23]       // 's'
```

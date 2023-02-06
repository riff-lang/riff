# Length Operator

When used as a prefix operator, `#` returns the length of a value. When
performed on string values, the result of the expression is the length of the
string *in bytes*. When performed on tables, the result of the expression is the
number of non-`null` values in the table.

```riff
s = 'string'
a = { 1, 2, 3, 4 }

#s; // 6
#a; // 4
```

The length operator can be used on numeric values as well; returning the length
of the number in decimal form.

```riff
#123;       // 3
#-230;      // 4
#0.6345;    // 6
#0x1f;      // 2
```

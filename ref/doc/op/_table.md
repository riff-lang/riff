--------------------------------------------------------------------------------
 Operator(s)  Description                             Associativity  Precedence
------------- --------------------------------------- -------------- -----------
`=`           Assignment                              Right          1

`?:`          Ternary conditional                     Right          2

`..`          Range constructor                       Left           3

`||`          Logical `OR`                            Left           4
<br>`or`

`&&`          Logical `AND`                           Left           5
<br>`and`

`==`          Relational equality, inequality         Left           6
<br>`!=`

`~`           Match, negated match                    Left           6
<br>`!~`

`<`           Relational comparison
<br>`<=`      $<$, $\leqslant$, $>$ and $\geqslant$   Left           7
<br>`>`
<br>`>=`

`|`           Bitwise `OR`                            Left           8

`^`           Bitwise `XOR`                           Left           9

`&`           Bitwise `AND`                           Left           10

`<<`          Bitwise left shift, right shift         Left           11
<br>`>>`

`#`           Concatenation                           Left           11

`+`           Addition, subtraction                   Left           12
<br>`-`

`*`           Multiplication, division, remainder     Left           13
<br>`/`
<br>`%`

`!`           Logical `NOT`                           Right          13
<br>`not`

`#`           Length                                  Right          13

`+`           Unary plus, minus                       Right          13
<br>`-`

`~`           Bitwise `NOT`                           Right          13

`**`          Exponentiation                          Right          15

`++`          Prefix increment, decrement             Right          15
<br>`--`

`()`          Function call                           Left           16

`[]`          Subscripting                            Left           16

`.`           Member access                           Left           16

`++`          Postfix increment, decrement            Left           16
<br>`--`

`$`           Field table subscripting                Right          17
--------------------------------------------------------------------------------

: Operators (increasing in precedence)

Riff also supports the following compound assignment operators, with the same
precedence and associativity as simple assignment (`=`)

```
+=      |=
&=      **=
#=      <<=
/=      >>=
%=      -=
*=      ^=
```

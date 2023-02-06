# Basic Concepts

A Riff program is a sequence of statements. Riff has no concept of statement
terminators. The lexical analysis phase does not perform implicit semicolon
insertion. A statement ends when the next lexical token in the token stream is
not applicable to the current statement.

Variables are global by default. Riff allows local variable usage by explicitly
declaring a variable with the [`local`] keyword. Riff also allows the
use/access of uninitialized variables. When an uninitialized variable is used,
Riff reserves the variable with global scope and initializes it to `null`.
Depending on the context, the variable may also be initialized to `0` or an
empty table. Riff does not allow uninitialized variables to be called as
functions.

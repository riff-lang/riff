# `eval(s)` {#eval}

Compiles and executes the string `s` as Riff code. Global state is inherited and
can be altered by the code `s`. `eval()` currently has no access to variables in
local scope.

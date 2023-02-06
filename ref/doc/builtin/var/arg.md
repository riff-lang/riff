# `arg` Table {#arg}

Whenever `riff` is invoked, it collects all the command-line arguments and
stores them as string literals in a Riff table named `arg`. `arg[1]` will
always be the first user-provided argument following the program text or program
filename. For example, when invoking `riff` on the command-line like this:

```bash
$ riff -e 'arg[1] << arg[2]' 2 3
```

The `arg` table will be populated as follows:

```
arg[-2]: 'riff'
arg[-1]: '-e'
arg[0]:  'arg[1] << arg[2]'
arg[1]:  '2'
arg[2]:  '3'
```

Another example, this time with a Riff program stored in a file name
`prog.rf`:

```bash
$ riff prog.rf 43 22
```

The `arg` table would be populated:

```
arg[-1]: 'riff'
arg[0]:  'prog.rf'
arg[1]:  '43'
arg[2]:  '22'
```

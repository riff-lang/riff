# Riff

Riff is a general-purpose programming language designed primarily for
prototyping and command-line usage. Riff offers a familiar syntax
similar to many C-style languages along with some extra conveniences
to make it a useful supplementary tool for programmers.

This repository contains source code for the Riff interpreter.

## Status

Riff is still in the design/prototype phase. Documentation and
language specification can be found on [Riff's
website](https://riff.cx/doc).

## Usage

Running `make` in the root directory of the repository will compile
the source code and place the executable in `bin/riff`.

Running `make install` will compile the source code and place the
executable at `/usr/local/bin/riff`

- Run a Riff program on the command-line:

    ```bash
    $ riff '<program>'
    ```

- Run a Riff program stored in a file:

    ```bash
    $ riff -f file.rf
    ```

- Print a listing of mnemonics which associate to `riff` VM
  instructions for a given program:
  
    ```bash
    $ riff -l '<program>'
    $ riff -lf file.rf
    ```

# Riff

Riff is a general-purpose programming language designed primarily for
prototyping and command-line usage. Riff offers a familiar syntax
similar to many C-style languages along with some extra conveniences
to make it a useful supplementary tool for programmers.

This repository contains source code for the Riff interpreter.

## Status

Riff is still in the design/prototype phase. Documentation and
language specification is currently non-existent. Feel free to browse
through the [examples](ex) to get a general idea of the language and
syntax.

## Usage

Running `make` in the root directory of the repository will compile
the source code and create an executable named `a.out`.

- Run a Riff program on the command-line:

    ```bash
    $ ./a.out '<program>'
    ```

- Run a Riff program stored in a file:

    ```bash
    $ ./a.out -f file.rf
    ```

- Print a listing of mnemonics which associate to `riff` VM
  instructions for a given program:
  
    ```bash
    $ ./a.out -l '<program>'
    $ ./a.out -lf file.rf
    ```

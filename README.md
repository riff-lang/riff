# Riff

Riff is a general-purpose programming language designed primarily for
prototyping and command-line usage. Riff offers a familiar syntax
similar to many C-style languages along with some extra conveniences
to make it a useful supplementary tool for programmers.

This repository contains source code for the Riff interpreter.

Please refer to [Riff's website](https://riff.cx) for documentation
and language specification. There is also a [standalone
playground](https://riff.run) featuring sample programs.

## Building

Riff depends on the [Perl Compatible Regular
Expressions](http://pcre.org) (PCRE2) library. PCRE2 will need to be
installed on your system. Specifically `pcre2-config`, which retrieves
the installation location of PCRE2. PCRE2 can be easily installed via
many popular package managers.

Running `make` in the root directory of the repository will compile
the source code and place the executable locally at `bin/riff`.

Running `make install` will compile the source code and place the
executable at `/usr/local/bin/riff`.

## Basic Usage

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

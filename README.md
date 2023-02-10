# Riff

Riff is a general-purpose programming language designed primarily for
prototyping and command-line usage. Riff offers a familiar syntax
similar to many C-style languages along with some extra conveniences
to make it a useful supplementary tool for programmers.

This repository contains source code for the Riff interpreter.

Please refer to [Riff's website](https://riff.cx) for documentation
and language specification. There is also a [standalone
playground](https://riff.run) featuring sample programs.

## Basic Usage

- Run a Riff program stored in a file:

    ```bash
    $ riff file.rf
    ```

- Run a Riff program on the command-line:

    ```bash
    $ riff -e '<program>'
    ```

- Print a listing of mnemonics which associate to `riff` VM
  instructions for a given program:
  
    ```bash
    $ riff -l file.rf
    $ riff -le '<program>'
    ```

## Building

Running `make` in the root directory of the repository will compile
the source code and place the executable locally at `bin/riff`.

Running `make install` will compile the source code and place the executable at
`~/.local/bin/riff` by default. To change the installation path, specify the
`prefix` when running `make install`.

For example, to install `riff` at `/usr/local/bin/riff`:

```bash
$ make install prefix=/usr/local
```

### Versioning

Riff utilizes [Git tags](https://git-scm.com/book/en/v2/Git-Basics-Tagging) to
generate the version string as seen when running `riff -v`. Building Riff from
source without the necessary Git metadata will result in a Riff binary with an
empty version string.

### Dependencies

Riff depends on the [Perl Compatible Regular
Expressions](http://pcre.org) (PCRE2) library. PCRE2 10.35 or higher
will need to be installed on your system. Specifically `pcre2-config`,
which retrieves the installation location of PCRE2.  PCRE2 can be
easily installed via many popular package managers.

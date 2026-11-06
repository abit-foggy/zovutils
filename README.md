# zovutils

[![Parity CI](https://github.com/abit-foggy/zovutils/actions/workflows/parity.yml/badge.svg?branch=master)](https://github.com/abit-foggy/zovutils/actions/workflows/parity.yml)
![Version 0.1.182](https://img.shields.io/badge/version-0.1.182-blue)
![Alpha](https://img.shields.io/badge/status-alpha-orange)
![GNU Coreutils target 9.11](https://img.shields.io/badge/target-GNU%20Coreutils%209.11-lightgrey)
![Zov 0.9.0](https://img.shields.io/badge/language-Zov%200.9.0-6f42c1)

`zovutils` is a multicall command-line utilities project written in Zov. It
targets GNU Coreutils 9.11 behavior and compiles through Zov to C99, then to a
native executable with libtcc.

GNU Coreutils is used only as an executable parity-test oracle. No GNU source
is copied, ported, or linked into this project.

## Status

The project is in alpha (`0.1.182`). The current command set is:

- `basename`
- `cat`
- `dirname`
- `echo`
- `env`
- `false`
- `head`
- `printenv`
- `printf`
- `pwd`
- `tail`
- `true`
- `wc`

Each command is available through `zovutils <command>` and, after installation,
through a command-name symlink. Compatibility work is ongoing; passing the
current test suite is not a claim of complete GNU Coreutils compatibility.

The parity workflow is configured for Linux and macOS on `x86_64` and
`aarch64`. See the [compatibility policy](docs/compatibility.md) for test scope,
platform behavior, and coverage limits.

## Build and run

Install Zov 0.9.0, then run:

```sh
zovc check
zovc build
./target/zovutils cat README.md
./target/zovutils wc -l README.md
```

To install `zovutils` and its command symlinks under `/usr/local/bin`:

```sh
make install
```

Set `PREFIX` to install elsewhere, for example `make install PREFIX="$HOME/.local"`.

## Development and parity tests

Run the unit and GNU parity suites with:

```sh
make unit
GNU_COREUTILS_DIR=/path/to/gnu-coreutils-9.11/bin make parity
```

Or run both with `make test`. The parity runner requires GNU Coreutils 9.11,
sets `LC_ALL=C` and `TZ=UTC0` for base cases, and compares stdout, stderr, and
exit status byte-for-byte. Locale-sensitive `wc` and diagnostic cases also run
under an installed UTF-8 locale.

On Linux with Clang, run the sanitizer parity suite with:

```sh
GNU_COREUTILS_DIR=/path/to/gnu-coreutils-9.11/bin make asan-parity
```

Command cases live in separate Python scripts under `tests/parity/`; shared
fixtures and comparison helpers live beside them. CI builds GNU Coreutils 9.11
from its checksum-verified release archive and runs the same checks across the
supported platform matrix.

## Roadmap

1. **Shell basics:** `basename`, `dirname`, `echo`, `env`, `false`, `printenv`,
   `printf`, `pwd`, `seq`, `sleep`, `test`, `true`, and `yes`.
2. **Text processing:** `cut`, `paste`, `sort`, `uniq`, `tr`, `expand`,
   `unexpand`, `nl`, `fold`, `fmt`, `join`, `comm`, `csplit`, `split`, `tac`,
   and `od`.
3. **Filesystem operations:** file and directory creation, copying, moving,
   linking, removal, installation, path resolution, permissions, and metadata.
4. **Data and system utilities:** checksums, encoding, numeric conversion,
   date and time, identity, environment, and host information.
5. **Specialized utilities:** device and terminal tools, buffering,
   privilege-sensitive commands, and the remaining GNU 9.11 utilities.

The project and required Zov compiler stay in the `0.x` alpha series. Minor
versions mark command-family additions; patch versions mark fixes.

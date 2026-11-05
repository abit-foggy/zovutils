# zovutils

`zovutils` is a Zov implementation of GNU Coreutils behavior. It compiles
through `zovc` to C99 and uses GNU Coreutils only as an executable parity
oracle; no GNU implementation source is copied, ported, or linked.

## Current status

The project is alpha (`0.1.74`) and targets GNU Coreutils 9.11 behavior. The
first command family is byte-stream processing: `cat`, `head`, `tail`, and
`wc`. Compatibility is incomplete until the differential suite covers every
supported command and behavior.

## Build and run

```sh
zovc check
zovc build
./target/zovutils cat README.md
./target/zovutils wc -l README.md
```

After `make install`, command-name symlinks dispatch to the shared executable:

```sh
cat README.md
wc -l README.md
```

## Development checks

```sh
make unit
GNU_COREUTILS_DIR=/path/to/gnu-coreutils-9.11/bin make parity
```

The Python parity suite requires GNU Coreutils 9.11, uses `LC_ALL=C` and
`TZ=UTC0` for its base cases, and compares output bytes, diagnostics, and exit
status. `wc` also runs locale-sensitive cases under an installed UTF-8 locale.
Command cases live in separate scripts under `tests/parity/`. See
[compatibility policy](docs/compatibility.md). GitHub Actions runs the suite on
Linux and macOS across x86_64 and aarch64.

## Roadmap

1. Shell basics: `basename`, `dirname`, `echo`, `env`, `false`, `printenv`,
   `printf`, `pwd`, `seq`, `sleep`, `test`, `true`, and `yes`.
2. Text processing: `cut`, `paste`, `sort`, `uniq`, `tr`, `expand`,
   `unexpand`, `nl`, `fold`, `fmt`, `join`, `comm`, `csplit`, `split`, `tac`,
   and `od`.
3. Filesystem operations, including path, metadata, and permission tools.
4. Data and system utilities, including checksums, encoding, numeric, date,
   identity, environment, and host information tools.
5. Specialized and privilege-sensitive utilities, then remaining GNU 9.11
   commands.

Feature-family releases remain `0.x` alpha. Minor versions mark added command
families; patch versions mark fixes.

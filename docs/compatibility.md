# Compatibility and platform policy

The behavior target is GNU Coreutils 9.11. GNU executables are used only as
test oracles; this project does not copy, port, or link GNU Coreutils source.
The Python parity suite is split into shared support and command-specific
scripts under `tests/parity/`. It requires a directory of GNU 9.11 executables
in `GNU_COREUTILS_DIR` and checks the oracle version before running cases.

Parity tests set `LC_ALL=C` and `TZ=UTC0` so byte and time output is stable.
Each case compares standard output, standard error, and exit status. Fixtures
cover empty input, binary bytes, missing final newlines, multiple files,
missing files, and large streamed inputs. Platform-dependent cases will run on
the same host for both implementations.

The initial supported targets follow Zov: Linux and macOS on `x86_64` and
`aarch64`. OS calls stay behind `src/shared/` helpers so support can expand
when Zov's hosted runtime supports more platforms.

The current command coverage is `cat`, `head`, `tail`, and `wc`; this does not
yet claim GNU Coreutils compatibility. See the project README for the planned
command families.

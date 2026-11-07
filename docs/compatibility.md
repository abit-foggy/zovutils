# Compatibility and platform policy

The behavior target is GNU Coreutils 9.11. GNU executables are used only as
test oracles; this project does not copy, port, or link GNU Coreutils source.
The Python parity suite is split into shared support and command-specific
scripts under `tests/parity/`. It requires a directory of GNU 9.11 executables
in `GNU_COREUTILS_DIR` and checks the oracle version before running cases.

Base parity tests set `LC_ALL=C`, `TZ=UTC0`, and `TERM=dumb` so byte, time, and
help output is stable. `wc` also checks character counts, whitespace, invalid
sequences, and display widths under an installed UTF-8 locale. Each case
compares standard output, standard error, and exit status. Utility `--version`
output identifies zovutils and its package version; it is checked separately
from GNU output. Fixtures cover empty input, binary bytes, missing final
newlines, multiple files, missing files, and large streamed inputs.
Platform-dependent cases run both implementations on the same host.

`tail --debug` reports the selected follow backend. On Linux, zovutils uses
inotify for watchable regular files and falls back to polling when notifications
are unavailable; other platforms use polling. The parity case compares this
diagnostic with GNU on the same host.

A separate Ubuntu Linux CI job runs root-only `mknod` and `chroot` success cases inside temporary fixtures. When SELinux is enforcing, it also compares same-context `chcon` and `runcon` success paths; otherwise those cases are reported as unavailable on that runner.

The initial supported targets follow Zov: Linux and macOS on `x86_64` and
`aarch64`. The `GNU Coreutils parity` GitHub Actions workflow builds GNU 9.11
from its checksum-verified release archive and runs the full suite on each
target. It pins the Zov compiler source revision used by the workflow. OS calls
stay behind `src/shared/` helpers so support can expand when Zov's hosted
runtime supports more platforms.

The implemented command set is listed in the project README. The
[compatibility inventory](compatibility-matrix.md) records the GNU 9.11 option
surface, associated parity suite, current suite case count, and audit status for
each executable name. Its option list is extracted from the pinned oracle's
`--help` output; it is a checklist, not evidence that those options have passed.
A utility is complete only after its documented options and relevant
interactions, diagnostics, side effects, and platform cases have been audited
and pass. The existing suite passes 17,674 cases locally, but current coverage
does not yet establish full compatibility.

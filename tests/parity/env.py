"""Parity cases for env."""

from __future__ import annotations

import os
from pathlib import Path

from support import ParityFailure, Suite


BASE_ENV = {
    "PATH": "/usr/bin:/bin",
    "LC_ALL": "C",
    "LANG": "C",
    "TZ": "UTC0",
    "TERM": "dumb",
    "KEEP": "kept",
    "WORD": "hello world",
}


def case(s: Suite, args: list[str], *, extra_env: dict[str, str] | None = None) -> None:
    child_env = BASE_ENV.copy()
    if extra_env:
        child_env.update(extra_env)
    child_env["PATH"] = str(s.gnu_dir) + os.pathsep + child_env["PATH"]
    s.count += 1
    zov = s._run([str(s.zovutils), "env", *args], b"", env=child_env)
    gnu = s._run(["env", *args], b"", env=child_env)
    if zov != gnu:
        raise ParityFailure(f"env {' '.join(args)} differs: zov={zov!r}, GNU={gnu!r}")


def run(s: Suite) -> None:
    denied = Path(s.path("env-not-executable"))
    denied.write_text("exit 0\n")
    denied.chmod(0o600)
    malformed = Path(s.path("env-invalid-executable"))
    malformed.write_text("plain text, not a program\n")
    malformed.chmod(0o700)
    for args in (
        [],
        ["-0"],
        ["--null"],
        ["A=one", "B=two"],
        ["-i"],
        ["--ignore-environment", "A=one"],
        ["-u", "KEEP"],
        ["--unset=KEEP"],
        ["-uKEEP"],
        ["-i", "A=one"],
        ["A=one", "/bin/sh", "-c", "printf '%s:%s' \"$A\" \"$KEEP\""],
        ["-i", "A=one", "/bin/sh", "-c", "printf '%s:%s' \"$A\" \"${KEEP-unset}\""],
        ["-C", "/tmp", "/bin/sh", "-c", "pwd"],
        ["--chdir=/tmp", "/bin/sh", "-c", "pwd"],
        ["-a", "custom-zero", "/bin/sh", "-c", "printf %s \"$0\""],
        ["--argv0=custom-zero", "/bin/sh", "-c", "printf %s \"$0\""],
        ["-v", "A=one", "/bin/true"],
        ["--debug", "-i", "A=one", "/bin/true"],
        ["-v", "-C", "/tmp", "/bin/true"],
        ["-v", "--argv0=custom-zero", "/bin/true", "arg"],
        ["--ignore-signal=PIPE", "--list-signal-handling", "/bin/true"],
        ["--ignore-signal=sigpipe", "--list-signal-handling", "/bin/true"],
        ["--block-signal=13", "--list-signal-handling", "/bin/true"],
        ["--default-signal=PIPE", "--list-signal-handling", "/bin/true"],
        ["--ignore-signal=", "--list-signal-handling", "/bin/true"],
        ["--ignore-signal=INVALID", "/bin/true"],
        ["-S", "echo one\\_two"],
        ["--split-string=echo \"two words\""],
        ["-S", "echo 'two words' three"],
        ["-S", "printf '%s' ${WORD}"],
        ["-S", "printf '<%s>'", "extra"],
        ["-S", "echo a # ignored words"],
        ["-S", r"echo a\#b"],
        ["-S", r"echo a\qb"],
        ["-S", "echo $WORD"],
        ["-S", 'echo "unterminated'],
        ["/bin/sh", "-c", "exit 7"],
        ["/no/such/zovutils-command"],
        [str(denied)],
        [str(malformed)],
        ["-0", "/bin/true"],
        ["--bad-option"],
        ["--d"],
        ["--i"],
        ["--de=value"],
        ["-u"],
        ["--unset"],
        ["-C"],
        ["--chdir"],
        ["-S"],
        ["--null=value"],
    ):
        case(s, args)
    case(s, ["--help"])
    s.product_version("env")

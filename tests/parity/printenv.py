"""Parity cases for printenv."""

from __future__ import annotations

import os

from support import ParityFailure, Suite


def case(s: Suite, args: list[str]) -> None:
    child_env = s.env.copy()
    child_env.update({"KEEP": "kept", "WORD": "hello world", "EMPTY": ""})
    child_env["PATH"] = str(s.gnu_dir) + os.pathsep + child_env.get("PATH", "")
    zov = s._run([str(s.zovutils), "printenv", *args], b"", env=child_env)
    gnu = s._run(["printenv", *args], b"", env=child_env)
    s.count += 1
    if zov != gnu:
        raise ParityFailure(f"printenv {' '.join(args)} differs: zov={zov!r}, GNU={gnu!r}")


def run(s: Suite) -> None:
    for args in (
        [],
        ["-0"],
        ["--null"],
        ["KEEP"],
        ["WORD", "KEEP"],
        ["EMPTY"],
        ["MISSING"],
        ["KEEP", "MISSING", "WORD"],
        ["--", "-0"],
        ["-00", "KEEP"],
        ["--bad-option"],
        ["-x"],
        ["--null=value"],
        ["--help=value"],
        ["--version=value"],
        ["--n"],
    ):
        case(s, args)

    # These commands are handled separately because the GNU version string
    # describes the oracle, while the product reports its own package version.
    case(s, ["--help"])
    s.product_version("printenv")
    s.symlink_dispatch("printenv", ["KEEP"])

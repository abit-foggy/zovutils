"""Parity cases for mktemp."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

from support import Suite


def _created(s: Suite, args: list[str], *, directory: bool = False) -> None:
    outputs = []
    for command, env in (
        ([str(s.zovutils), "mktemp", *args], s.env),
        (["mktemp", *args], {**s.env, "PATH": str(s.gnu_dir) + os.pathsep + os.environ.get("PATH", "")}),
    ):
        result = subprocess.run(command, cwd=s.root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        path = result.stdout.decode().rstrip("\n")
        created = Path(path)
        if not created.is_absolute():
            created = s.root / created
        ok = created.is_dir() if directory and result.returncode == 0 else created.is_file() if result.returncode == 0 else False
        outputs.append((result.returncode, result.stderr, created.name, ok))
    s.count += 1
    if outputs[0][0:2] != outputs[1][0:2] or not outputs[0][3] or not outputs[1][3]:
        raise AssertionError(f"mktemp creation differs for {args!r}: {outputs!r}")


def run(s: Suite) -> None:
    for args in (["--bad-option"], ["abc"], ["--suffix=/bad", "tmp.XXXXXX"]):
        s.case("mktemp", args)
    s.product_version("mktemp")
    s.symlink_dispatch("mktemp", ["--help"])
    _created(s, ["file.XXXXXXXXXX"])
    _created(s, ["--directory", "dir.XXXXXXXXXX"], directory=True)
    _created(s, [f"--tmpdir={s.root}", "tmp.XXXXXXXXXX"])
    _created(s, ["--suffix=.log", "tmp.XXXXXXXXXX"])
    _created(s, [])

"""Parity cases for mkfifo."""

from __future__ import annotations

import os
import stat
import subprocess
import tempfile
from pathlib import Path

from support import Suite


def _case(s: Suite, args: list[str], *, expected_mode: int | None = None, setup=None) -> None:
    roots = [tempfile.mkdtemp(prefix=f"mkfifo-{label}-", dir=s.root) for label in ("zov", "gnu")]
    outcomes = []
    modes = []
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    for index, root in enumerate(roots):
        if setup is not None:
            setup(root)
        command = [str(s.zovutils), "mkfifo", *args] if index == 0 else ["mkfifo", *args]
        result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        outcomes.append((result.returncode, result.stdout, result.stderr))
        path = Path(root, "pipe")
        modes.append(stat.S_IMODE(path.stat().st_mode) if path.exists() and stat.S_ISFIFO(path.stat().st_mode) else None)
    s.count += 1
    if outcomes[0] != outcomes[1] or (expected_mode is not None and modes != [expected_mode, expected_mode]):
        raise AssertionError(f"mkfifo behavior differs for {args!r}: {outcomes!r}, modes={modes!r}")


def run(s: Suite) -> None:
    for args in ([], ["--bad-option"], ["-m"], ["-m", "bad", "pipe"], ["--help"], ["--mode=bad", "pipe"], ["--help=bad"]):
        s.case("mkfifo", args)
    s.product_version("mkfifo")
    s.symlink_dispatch("mkfifo", ["--help"])
    _case(s, ["pipe"], expected_mode=0o644)
    _case(s, ["-m", "600", "pipe"], expected_mode=0o600)
    _case(s, ["-m", "u=rw,go=", "pipe"], expected_mode=0o600)
    _case(s, ["pipe", "other"], setup=lambda root: Path(root, "pipe").write_bytes(b"x"))

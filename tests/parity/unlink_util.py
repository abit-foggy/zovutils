"""Parity cases for unlink."""

from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

from support import Suite


def _case(s: Suite, args: list[str], *, setup=None, expected_removed: bool | None = None) -> None:
    roots = [tempfile.mkdtemp(prefix=f"unlink-{label}-", dir=s.root) for label in ("zov", "gnu")]
    outcomes = []
    removed = []
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    for index, root in enumerate(roots):
        if setup is not None:
            setup(root)
        command = [str(s.zovutils), "unlink", *args] if index == 0 else ["unlink", *args]
        result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        outcomes.append((result.returncode, result.stdout, result.stderr))
        removed.append(not Path(root, "item").exists() and not Path(root, "item").is_symlink())
    s.count += 1
    if outcomes[0] != outcomes[1] or (expected_removed is not None and removed != [expected_removed, expected_removed]):
        raise AssertionError(f"unlink behavior differs for {args!r}: {outcomes!r}, removed={removed!r}")


def run(s: Suite) -> None:
    for args in ([], ["one", "two"], ["--bad-option"], ["--help"], ["--help=bad"], ["--version=bad"]):
        s.case("unlink", args)
    s.product_version("unlink")
    s.symlink_dispatch("unlink", ["--help"])
    _case(s, ["item"], setup=lambda root: Path(root, "item").write_bytes(b"x"), expected_removed=True)
    _case(s, ["item"], setup=lambda root: os.symlink("missing", Path(root, "item")), expected_removed=True)
    _case(s, ["item"], expected_removed=True)
    _case(s, ["item"], setup=lambda root: Path(root, "item").mkdir(), expected_removed=False)

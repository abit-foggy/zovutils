"""Parity cases for link."""

from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

from support import Suite


def _success_case(s: Suite, args: list[str]) -> None:
    roots = [tempfile.mkdtemp(prefix=f"link-{label}-", dir=s.root) for label in ("zov", "gnu")]
    outcomes = []
    inode_checks = []
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    for index, root in enumerate(roots):
        Path(root, "source").write_bytes(b"payload")
        command = [str(s.zovutils), "link", *args] if index == 0 else ["link", *args]
        result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        outcomes.append((result.returncode, result.stdout, result.stderr))
        inode_checks.append(os.stat(Path(root, "dest")).st_ino == os.stat(Path(root, "source")).st_ino)
    s.count += 1
    if outcomes[0] != outcomes[1] or inode_checks != [True, True]:
        raise AssertionError(f"link filesystem behavior differs for {args!r}: {outcomes!r}, hardlinks={inode_checks!r}")


def run(s: Suite) -> None:
    for args in ([], ["source"], ["source", "dest", "extra"], ["--bad-option"], ["--help"], ["--help=bad"], ["--version=bad"]):
        s.case("link", args)
    s.case("link", ["/path/that/does/not/exist", "dest"])
    s.product_version("link")
    s.symlink_dispatch("link", ["--help"])
    _success_case(s, ["source", "dest"])
    _success_case(s, ["--", "source", "dest"])

"""Filesystem parity cases for renames and moves."""

from __future__ import annotations

import os
import stat
import subprocess
import tempfile
from pathlib import Path
from typing import Callable

from support import Suite


def _tree(root: str) -> list[tuple[object, ...]]:
    result: list[tuple[object, ...]] = []
    for current, dirs, files in os.walk(root, followlinks=False):
        dirs.sort()
        files.sort()
        for name in dirs + files:
            path = os.path.join(current, name)
            info = os.lstat(path)
            if stat.S_ISDIR(info.st_mode):
                kind, detail = "dir", ""
            elif stat.S_ISREG(info.st_mode):
                kind, detail = "file", Path(path).read_bytes()
            elif stat.S_ISLNK(info.st_mode):
                kind, detail = "link", os.readlink(path)
            else:
                kind, detail = "other", ""
            result.append((os.path.relpath(path, root), kind, stat.S_IMODE(info.st_mode), detail))
    return result


def _case(
    s: Suite,
    args: list[str],
    setup: Callable[[str], None] | None = None,
    *,
    stdin: bytes = b"",
) -> None:
    left = tempfile.mkdtemp(prefix="mv-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="mv-gnu-", dir=s.root)
    for root in (left, right):
        if setup is not None:
            setup(root)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run([str(s.zovutils), "mv", *args], cwd=left, env=env, input=stdin,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    gnu = subprocess.run(["mv", *args], cwd=right, env=env, input=stdin,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    s.count += 1
    got = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    expected = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if got != expected:
        raise AssertionError(f"mv filesystem behavior differs for {args!r}:\nzov={got!r}\ngnu={expected!r}")


def _files(root: str) -> None:
    Path(root, "source").write_bytes(b"move\0bytes")


def _directory(root: str) -> None:
    os.mkdir(os.path.join(root, "source"))
    Path(root, "source", "child").write_bytes(b"nested")


def run(s: Suite) -> None:
    s.case("mv", [])
    s.case("mv", ["--unknown"])
    s.case("mv", ["--target-directory"])
    s.case("mv", ["--help"])
    s.product_version("mv")
    s.symlink_dispatch("mv", [])

    _case(s, ["source", "destination"], _files)
    _case(s, ["-v", "source", "destination"], _files)
    _case(s, ["source", "destination"], lambda root: (_files(root), Path(root, "destination").write_bytes(b"old")))
    _case(s, ["-n", "source", "destination"], lambda root: (_files(root), Path(root, "destination").write_bytes(b"old")))
    _case(s, ["-i", "source", "destination"], lambda root: (_files(root), Path(root, "destination").write_bytes(b"old")), stdin=b"n\n")
    _case(s, ["-i", "source", "destination"], lambda root: (_files(root), Path(root, "destination").write_bytes(b"old")), stdin=b"y\n")
    _case(s, ["source", "copies"], lambda root: (_files(root), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["-t", "copies", "source"], lambda root: (_files(root), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["source", "second", "copies"], lambda root: (_files(root), Path(root, "second").write_bytes(b"2"), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["source", "missing"], _files)
    _case(s, ["missing", "destination"])
    _case(s, ["source", "source"], _files)
    _case(s, ["-T", "source", "directory"], lambda root: (_files(root), os.mkdir(os.path.join(root, "directory"))))
    _case(s, ["source", "moved"], lambda root: (os.symlink("target", os.path.join(root, "source"))))
    _case(s, ["source", "destination"], _directory)

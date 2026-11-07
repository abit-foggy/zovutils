"""Filesystem parity cases for removal and recursive traversal."""

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
    left = tempfile.mkdtemp(prefix="rm-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="rm-gnu-", dir=s.root)
    for root in (left, right):
        if setup is not None:
            setup(root)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run([str(s.zovutils), "rm", *args], cwd=left, env=env, input=stdin,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    gnu = subprocess.run(["rm", *args], cwd=right, env=env, input=stdin,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    s.count += 1
    got = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    expected = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if got != expected:
        raise AssertionError(f"rm filesystem behavior differs for {args!r}:\nzov={got!r}\ngnu={expected!r}")


def _files(root: str) -> None:
    Path(root, "file").write_bytes(b"data")
    os.symlink("file", os.path.join(root, "link"))


def _tree_fixture(root: str) -> None:
    os.makedirs(os.path.join(root, "tree", "sub"))
    Path(root, "tree", "file").write_bytes(b"one")
    Path(root, "tree", "sub", "deep").write_bytes(b"two")


def _nonempty_directory(root: str) -> None:
    os.mkdir(os.path.join(root, "directory"))
    Path(root, "directory", "file").write_text("contents")


def run(s: Suite) -> None:
    s.case("rm", [])
    s.case("rm", ["--unknown"])
    s.case("rm", ["--interactive=bad", "file"])
    s.case("rm", ["--help"])
    s.product_version("rm")
    s.symlink_dispatch("rm", [])

    _case(s, ["file"], _files)
    _case(s, ["-v", "file"], _files)
    _case(s, ["link"], _files)
    _case(s, ["missing"])
    _case(s, ["-f", "missing"])
    _case(s, ["directory"], lambda root: os.mkdir(os.path.join(root, "directory")))
    _case(s, ["-d", "directory"], lambda root: os.mkdir(os.path.join(root, "directory")))
    _case(s, ["-d", "directory"], _nonempty_directory)
    _case(s, ["-r", "tree"], _tree_fixture)
    _case(s, ["-rv", "tree"], _tree_fixture)
    _case(s, ["-r", "link"], _files)
    _case(s, ["file", "link"], _files)
    _case(s, ["-i", "file"], _files, stdin=b"n\n")
    _case(s, ["-i", "file"], _files, stdin=b"y\n")
    _case(s, ["-ri", "tree"], _tree_fixture, stdin=b"n\n")
    _case(s, ["-ri", "tree"], _tree_fixture, stdin=b"y\ny\ny\ny\ny\ny\n")
    _case(s, ["-I", "one", "two", "three", "four"], lambda root: [Path(root, n).write_text(n) for n in ("one", "two", "three", "four")], stdin=b"n\n")
    _case(s, ["-r", "/"])

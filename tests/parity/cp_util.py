"""Filesystem parity cases for file and directory copying."""

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
    left = tempfile.mkdtemp(prefix="cp-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="cp-gnu-", dir=s.root)
    for root in (left, right):
        if setup is not None:
            setup(root)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run([str(s.zovutils), "cp", *args], cwd=left, env=env, input=stdin,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    gnu = subprocess.run(["cp", *args], cwd=right, env=env, input=stdin,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    s.count += 1
    got = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    expected = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if got != expected:
        raise AssertionError(f"cp filesystem behavior differs for {args!r}:\nzov={got!r}\ngnu={expected!r}")


def _file(root: str) -> None:
    Path(root, "source").write_bytes(b"copy\0bytes\n")


def _directory(root: str) -> None:
    os.mkdir(os.path.join(root, "source"))
    Path(root, "source", "child").write_bytes(b"nested")
    os.mkdir(os.path.join(root, "source", "subdir"))
    Path(root, "source", "subdir", "deep").write_bytes(b"deep")


def _nested_file(root: str) -> None:
    os.makedirs(os.path.join(root, "nested", "path"))
    Path(root, "nested", "path", "file").write_bytes(b"nested")


def run(s: Suite) -> None:
    s.case("cp", [])
    s.case("cp", ["--unknown"])
    s.case("cp", ["--target-directory"])
    s.case("cp", ["--help"])
    s.product_version("cp")
    s.symlink_dispatch("cp", [])

    _case(s, ["source", "copy"], _file)
    _case(s, ["-v", "source", "copy"], _file)
    _case(s, ["source", "destination"], lambda root: (_file(root), Path(root, "destination").write_bytes(b"old")))
    _case(s, ["-b", "source", "destination"], lambda root: (_file(root), Path(root, "destination").write_bytes(b"old")))
    _case(s, ["--backup=numbered", "source", "destination"], lambda root: (_file(root), Path(root, "destination").write_bytes(b"old")))
    _case(s, ["-bS.bak", "source", "destination"], lambda root: (_file(root), Path(root, "destination").write_bytes(b"old")))
    _case(s, ["-n", "source", "destination"], lambda root: (_file(root), Path(root, "destination").write_bytes(b"old")))
    _case(s, ["-i", "source", "destination"], lambda root: (_file(root), Path(root, "destination").write_bytes(b"old")), stdin=b"n\n")
    _case(s, ["-i", "source", "destination"], lambda root: (_file(root), Path(root, "destination").write_bytes(b"old")), stdin=b"y\n")
    _case(s, ["--remove-destination", "source", "destination"], lambda root: (_file(root), os.symlink("missing", os.path.join(root, "destination"))))
    _case(s, ["-p", "source", "copy"], lambda root: (Path(root, "source").write_bytes(b"x"), os.chmod(os.path.join(root, "source"), 0o641)))
    _case(s, ["source", "missing-dir/copy"], _file)
    _case(s, ["missing", "copy"])
    _case(s, ["-t", "missing", "source"], _file)
    _case(s, ["source", "second", "missing"], lambda root: (_file(root), Path(root, "second").write_bytes(b"2")))
    _case(s, ["-T", "source", "destination"], lambda root: (_file(root), os.mkdir(os.path.join(root, "destination"))))
    _case(s, ["source", "source"], _file)
    _case(s, ["source", "copies"], lambda root: (_file(root), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["-t", "copies", "source"], lambda root: (_file(root), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["--parents", "nested/path/file", "copies"], lambda root: (_nested_file(root), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["source", "second", "copies"], lambda root: (_file(root), Path(root, "second").write_bytes(b"2"), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["-R", "source", "copy"], _directory)
    _case(s, ["-R", "source", "copies"], lambda root: (_directory(root), os.mkdir(os.path.join(root, "copies"))))
    _case(s, ["-R", "source", "copy"], lambda root: (os.mkdir(os.path.join(root, "source")), os.symlink("../target", os.path.join(root, "source", "link"))))
    _case(s, ["-R", "source", "source/new"], _directory)
    _case(s, ["-P", "link", "copy"], lambda root: (_file(root), os.symlink("source", os.path.join(root, "link"))))
    _case(s, ["-L", "link", "copy"], lambda root: (_file(root), os.symlink("source", os.path.join(root, "link"))))
    _case(s, ["link", "copy"], lambda root: (_file(root), os.symlink("source", os.path.join(root, "link"))))
    _case(s, ["-l", "source", "copy"], _file)
    _case(s, ["-s", "source", "copy"], _file)

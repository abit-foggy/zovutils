"""Filesystem parity cases for hard and symbolic links."""

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
            result.append((os.path.relpath(path, root), kind, stat.S_IMODE(info.st_mode), info.st_nlink, detail))
    return result


def _case(
    s: Suite,
    args: list[str],
    *,
    setup: Callable[[str], None] | None = None,
    stdin: bytes = b"",
    extra_env: dict[str, str] | None = None,
) -> None:
    left = tempfile.mkdtemp(prefix="ln-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="ln-gnu-", dir=s.root)
    for root in (left, right):
        if setup is not None:
            setup(root)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    if extra_env:
        env.update(extra_env)
    zov = subprocess.run(
        [str(s.zovutils), "ln", *args], cwd=left, env=env, input=stdin,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    gnu = subprocess.run(
        ["ln", *args], cwd=right, env=env, input=stdin,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    s.count += 1
    zov_result = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    gnu_result = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if zov_result != gnu_result:
        raise AssertionError(f"ln filesystem behavior differs for {args!r}:\nzov={zov_result!r}\ngnu={gnu_result!r}")


def _source(root: str) -> None:
    Path(root, "source").write_bytes(b"data\0bytes")


def _links(root: str) -> None:
    _source(root)
    os.symlink("source", os.path.join(root, "source-link"))


def run(s: Suite) -> None:
    s.case("ln", [])
    s.case("ln", ["--unknown"])
    s.case("ln", ["--target-directory"])
    s.case("ln", ["--backup=bad", "source", "destination"])
    s.case("ln", ["--help"])
    s.product_version("ln")
    s.symlink_dispatch("ln", [])

    _case(s, ["source", "hard"], setup=_source)
    _case(s, ["-s", "missing", "symbolic"])
    _case(s, ["-sv", "source", "symbolic"], setup=_source)
    _case(s, ["-v", "source", "hard"], setup=_source)
    _case(s, ["-sr", "source", "subdir/link"], setup=lambda root: (_source(root), os.mkdir(os.path.join(root, "subdir"))))
    _case(s, ["-r", "source", "symbolic"])
    _case(s, ["-r", "source", "destination"], setup=_source)
    _case(s, ["-t", "links", "source"], setup=lambda root: (_source(root), os.mkdir(os.path.join(root, "links"))))
    _case(s, ["source", "second", "links"], setup=lambda root: (Path(root, "source").write_text("a"), Path(root, "second").write_text("b"), os.mkdir(os.path.join(root, "links"))))
    _case(s, ["-f", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")))
    _case(s, ["-b", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")))
    _case(s, ["--backup=numbered", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")))
    _case(s, ["-f", "--backup=none", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")))
    _case(s, ["-bS.bak", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")))
    _case(s, ["-b", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")), extra_env={"VERSION_CONTROL": "numbered"})
    _case(s, ["--target-directory=links", "source"], setup=lambda root: (_source(root), os.mkdir(os.path.join(root, "links"))))
    _case(s, ["-s", "source", "links"], setup=lambda root: (_source(root), os.mkdir(os.path.join(root, "links"))))
    _case(s, ["-T", "source", "directory"], setup=lambda root: (_source(root), os.mkdir(os.path.join(root, "directory"))))
    _case(s, ["-t", "missing", "source"], setup=_source)
    _case(s, ["source", "second", "not-directory"], setup=lambda root: (Path(root, "source").write_text("a"), Path(root, "second").write_text("b")))
    _case(s, ["source", "second", "not-directory"], setup=lambda root: (Path(root, "source").write_text("a"), Path(root, "second").write_text("b"), Path(root, "not-directory").write_text("file")))
    _case(s, ["-T", "source", "second", "links"], setup=lambda root: (Path(root, "source").write_text("a"), Path(root, "second").write_text("b"), os.mkdir(os.path.join(root, "links"))))
    _case(s, ["-L", "source-link", "hard"], setup=_links)
    _case(s, ["-P", "source-link", "hard"], setup=_links)
    _case(s, ["-snf", "replacement", "directory-link"], setup=lambda root: (os.mkdir(os.path.join(root, "directory")), os.symlink("directory", os.path.join(root, "directory-link"))))
    _case(s, ["-n", "-s", "source", "directory-link"], setup=lambda root: (os.mkdir(os.path.join(root, "directory")), os.symlink("directory", os.path.join(root, "directory-link"))))
    _case(s, ["source", "missing"], setup=_source)
    _case(s, ["source", "existing"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "existing").write_text("old")))
    _case(s, ["source"], setup=_source)
    _case(s, ["--", "-source", "-destination"], setup=lambda root: Path(root, "-source").write_text("data"))
    _case(s, ["-i", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")), stdin=b"n\n")
    _case(s, ["-i", "source", "destination"], setup=lambda root: (Path(root, "source").write_text("new"), Path(root, "destination").write_text("old")), stdin=b"y\n")
    _case(s, ["-v", "space source", "space destination"], setup=lambda root: (Path(root, "space source").write_text("data")))

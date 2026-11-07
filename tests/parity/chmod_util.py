"""Permission parity cases for numeric, symbolic, and recursive chmod."""

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
            kind = "dir" if stat.S_ISDIR(info.st_mode) else "file" if stat.S_ISREG(info.st_mode) else "link"
            result.append((os.path.relpath(path, root), kind, stat.S_IMODE(info.st_mode)))
    return result


def _case(s: Suite, args: list[str], setup: Callable[[str], None] | None = None) -> None:
    left = tempfile.mkdtemp(prefix="chmod-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="chmod-gnu-", dir=s.root)
    for root in (left, right):
        if setup is not None:
            setup(root)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run([str(s.zovutils), "chmod", *args], cwd=left, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    gnu = subprocess.run(["chmod", *args], cwd=right, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    s.count += 1
    got = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    expected = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if got != expected:
        raise AssertionError(f"chmod behavior differs for {args!r}:\nzov={got!r}\ngnu={expected!r}")


def _files(root: str) -> None:
    Path(root, "file").write_text("content")
    os.chmod(os.path.join(root, "file"), 0o640)
    os.mkdir(os.path.join(root, "directory"))
    Path(root, "directory", "child").write_text("child")
    os.chmod(os.path.join(root, "directory", "child"), 0o600)


def _directory_link(root: str) -> None:
    os.makedirs(os.path.join(root, "target", "sub"))
    Path(root, "target", "sub", "file").write_text("data")
    os.symlink("target", os.path.join(root, "link"))


def run(s: Suite) -> None:
    s.case("chmod", [])
    s.case("chmod", ["--unknown"])
    s.case("chmod", ["--help"])
    s.product_version("chmod")
    s.symlink_dispatch("chmod", [])

    _case(s, ["755", "file"], _files)
    _case(s, ["u+x", "file"], _files)
    _case(s, ["g-w", "file"], _files)
    _case(s, ["a=rw", "file"], _files)
    _case(s, ["g=u", "file"], _files)
    _case(s, ["+X", "file"], _files)
    _case(s, ["+X", "directory"], _files)
    _case(s, ["a+t", "directory"], _files)
    _case(s, ["-R", "700", "directory"], _files)
    _case(s, ["-R", "o+r", "directory"], _files)
    _case(s, ["-v", "755", "file"], _files)
    _case(s, ["-c", "640", "file"], _files)
    _case(s, ["888", "file"], _files)
    _case(s, ["abc", "file"], _files)
    _case(s, ["755", "missing"])
    _case(s, ["755", "file", "directory"], _files)
    _case(s, ["--reference=file", "directory/child"], _files)
    _case(s, ["-R", "+X", "tree"], lambda root: (os.mkdir(os.path.join(root, "tree")), Path(root, "tree", "file").write_text("x")))
    _case(s, ["-R", "755", "link"], lambda root: (_files(root), os.symlink("file", os.path.join(root, "link"))))
    _case(s, ["-R", "700", "link"], _directory_link)
    _case(s, ["-R", "-P", "700", "link"], _directory_link)
    _case(s, ["-R", "-L", "700", "link"], _directory_link)
    _case(s, ["-h", "600", "link"], lambda root: (_files(root), os.symlink("file", os.path.join(root, "link"))))

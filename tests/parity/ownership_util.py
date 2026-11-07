"""Ownership parity cases for chown and chgrp."""

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
            result.append((os.path.relpath(path, root), kind, info.st_uid, info.st_gid,
                           os.readlink(path) if stat.S_ISLNK(info.st_mode) else None))
    return result


def _case(s: Suite, utility: str, args: list[str], setup: Callable[[str], None] | None = None) -> None:
    left = tempfile.mkdtemp(prefix=f"{utility}-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix=f"{utility}-gnu-", dir=s.root)
    for root in (left, right):
        if setup is not None:
            setup(root)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run([str(s.zovutils), utility, *args], cwd=left, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    gnu = subprocess.run([utility, *args], cwd=right, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False)
    s.count += 1
    got = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    expected = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if got != expected:
        raise AssertionError(f"{utility} differs for {args!r}:\nzov={got!r}\ngnu={expected!r}")


def _files(root: str) -> None:
    Path(root, "file").write_text("content")
    os.mkdir(os.path.join(root, "directory"))
    Path(root, "directory", "child").write_text("child")
    Path(root, "reference").write_text("reference")
    os.symlink("file", os.path.join(root, "link"))


def _directory_link(root: str) -> None:
    os.makedirs(os.path.join(root, "target", "sub"))
    Path(root, "target", "sub", "file").write_text("data")
    os.symlink("target", os.path.join(root, "link"))


def run(s: Suite) -> None:
    uid, gid = str(os.getuid()), str(os.getgid())
    for utility in ("chown", "chgrp"):
        s.case(utility, [])
        s.case(utility, ["--unknown"])
        s.case(utility, ["--help"])
        s.product_version(utility)
        s.symlink_dispatch(utility, [])

    _case(s, "chown", [uid, "file"], _files)
    _case(s, "chown", ["-v", uid, "file"], _files)
    _case(s, "chown", ["-v", f"{uid}:{gid}", "file"], _files)
    _case(s, "chown", ["-c", uid, "file"], _files)
    _case(s, "chown", [f"{uid}:{gid}", "file"], _files)
    _case(s, "chown", [f":{gid}", "file"], _files)
    _case(s, "chown", ["--reference=reference", "file"], _files)
    _case(s, "chown", ["--from", f"{uid}:{gid}", uid, "file"], _files)
    _case(s, "chown", ["--from", "999999999:999999999", uid, "file"], _files)
    _case(s, "chown", ["-R", f"{uid}:{gid}", "directory"], _files)
    _case(s, "chown", ["-R", uid, "link"], _directory_link)
    _case(s, "chown", ["-R", "-H", uid, "link"], _directory_link)
    _case(s, "chown", ["-R", "-L", uid, "link"], _directory_link)
    _case(s, "chown", ["-R", "-P", uid, "link"], _directory_link)
    _case(s, "chown", ["-h", uid, "link"], _files)
    _case(s, "chown", [uid, "missing"])
    _case(s, "chown", ["-f", uid, "missing"])
    _case(s, "chown", ["no-such-zovutils-user", "file"], _files)
    _case(s, "chown", [f"{uid}:no-such-zovutils-group", "file"], _files)
    _case(s, "chown", ["--reference=missing", "file"], _files)
    _case(s, "chown", ["--from=", uid, "file"], _files)
    _case(s, "chown", [uid])
    _case(s, "chown", ["-R", "--dereference", uid, "link"], _directory_link)
    _case(s, "chown", ["-R", "-H", "--dereference", uid, "link"], _directory_link)
    _case(s, "chown", ["--preserve-root", "-R", uid, "/"])

    _case(s, "chgrp", [gid, "file"], _files)
    _case(s, "chgrp", ["-v", gid, "file"], _files)
    _case(s, "chgrp", ["-c", gid, "file"], _files)
    _case(s, "chgrp", ["--reference=reference", "file"], _files)
    _case(s, "chgrp", ["-R", gid, "directory"], _files)
    _case(s, "chgrp", ["-R", gid, "link"], _directory_link)
    _case(s, "chgrp", ["-R", "-H", gid, "link"], _directory_link)
    _case(s, "chgrp", ["-R", "-L", gid, "link"], _directory_link)
    _case(s, "chgrp", ["-R", "-P", gid, "link"], _directory_link)
    _case(s, "chgrp", ["-h", gid, "link"], _files)
    _case(s, "chgrp", [gid, "missing"])
    _case(s, "chgrp", ["-f", gid, "missing"])
    _case(s, "chgrp", ["no-such-zovutils-group", "file"], _files)
    _case(s, "chgrp", ["--reference=missing", "file"], _files)
    _case(s, "chgrp", [gid])
    _case(s, "chgrp", ["-R", "--dereference", gid, "link"], _directory_link)
    _case(s, "chgrp", ["--preserve-root", "-R", gid, "/"])

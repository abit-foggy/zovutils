"""Parity cases for empty directory removal and parent processing."""

from __future__ import annotations

import os
import stat
import subprocess
import tempfile

from support import Suite


def _tree(root: str) -> list[tuple[str, str, int]]:
    result = []
    for current, dirs, files in os.walk(root):
        dirs.sort()
        files.sort()
        for name in dirs + files:
            path = os.path.join(current, name)
            info = os.lstat(path)
            kind = "dir" if stat.S_ISDIR(info.st_mode) else "file" if stat.S_ISREG(info.st_mode) else "other"
            result.append((os.path.relpath(path, root), kind, stat.S_IMODE(info.st_mode)))
    return result


def _case(s: Suite, args: list[str], *, nonempty_parent: bool = False, symlink: bool = False) -> None:
    left = tempfile.mkdtemp(prefix="rmdir-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="rmdir-gnu-", dir=s.root)
    for root in (left, right):
        os.makedirs(os.path.join(root, "a", "b"))
        if nonempty_parent:
            with open(os.path.join(root, "a", "keep"), "wb") as file:
                file.write(b"keep")
        if symlink:
            os.symlink("a", os.path.join(root, "link"))
        options_done = False
        for arg in args:
            if arg == "--":
                options_done = True
                continue
            if not options_done and arg.startswith("-"):
                continue
            destination = os.path.join(root, arg)
            if not os.path.lexists(destination):
                os.makedirs(destination, exist_ok=True)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run(
        [str(s.zovutils), "rmdir", *args], cwd=left, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    gnu = subprocess.run(
        ["rmdir", *args], cwd=right, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    s.count += 1
    zov_result = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    gnu_result = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if zov_result != gnu_result:
        raise AssertionError(
            f"rmdir filesystem behavior differs for {args!r}:\n"
            f"zov={zov_result!r}\ngnu={gnu_result!r}"
        )


def run(s: Suite) -> None:
    s.case("rmdir", [])
    s.case("rmdir", ["--unknown"])
    s.case("rmdir", ["--parents=value"])
    s.case("rmdir", ["--verbose=value"])
    s.case("rmdir", ["--ignore-fail-on-non-empty=value"])
    s.case("rmdir", ["-z"])
    s.case("rmdir", ["--help"])
    s.product_version("rmdir")
    s.symlink_dispatch("rmdir", [])

    _case(s, ["a/b"])
    _case(s, ["a/b", "a/b"])
    _case(s, ["-p", "a/b"])
    _case(s, ["-pv", "a/b"])
    _case(s, ["-v", "a/b"])
    _case(s, ["a/b"], nonempty_parent=True)
    _case(s, ["-p", "a/b"], nonempty_parent=True)
    _case(s, ["-pv", "a/b"], nonempty_parent=True)
    _case(s, ["--ignore-fail-on-non-empty", "-p", "a/b"], nonempty_parent=True)
    _case(s, ["--ignore-fail-on-non-empty", "a/b"], nonempty_parent=True)
    _case(s, ["--verbose", "a/b"])
    _case(s, ["-p", "link/b"], symlink=True)
    _case(s, ["--", "-leading-dash"])
    _case(s, ["-pv", "name with spaces"])
    _case(s, ["-pv", "apostrophe'name"])
    _case(s, ["-pv", "line\nbreak"])
    _case(s, ["-p", "a/./b"])

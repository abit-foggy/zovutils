"""Parity cases for directory creation, modes, and option handling."""

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


def _filesystem_case(
    s: Suite, args: list[str], *, existing_file: bool = False,
    existing_dir: str | None = None, existing_symlink: str | None = None
) -> None:
    left = tempfile.mkdtemp(prefix="mkdir-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="mkdir-gnu-", dir=s.root)
    if existing_file:
        for root in (left, right):
            with open(os.path.join(root, "block"), "wb") as file:
                file.write(b"file")
    if existing_dir is not None:
        for root in (left, right):
            os.mkdir(os.path.join(root, existing_dir))
    if existing_symlink is not None:
        for root in (left, right):
            os.mkdir(os.path.join(root, "target"))
            os.symlink("target", os.path.join(root, existing_symlink))

    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run(
        [str(s.zovutils), "mkdir", *args], cwd=left, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    gnu = subprocess.run(
        ["mkdir", *args], cwd=right, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    s.count += 1
    zov_result = (zov.returncode, zov.stdout, zov.stderr, _tree(left))
    gnu_result = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right))
    if zov_result != gnu_result:
        raise AssertionError(
            f"mkdir filesystem behavior differs for {args!r}:\n"
            f"zov={zov_result!r}\ngnu={gnu_result!r}"
        )


def run(s: Suite) -> None:
    s.case("mkdir", [])
    s.case("mkdir", ["--unknown"])
    s.case("mkdir", ["--mode"])
    s.case("mkdir", ["--verbose=value"])
    s.case("mkdir", ["--parents=value"])
    s.case("mkdir", ["--help=value"])
    s.case("mkdir", ["--version=value"])
    s.case("mkdir", ["--help"])
    s.product_version("mkdir")
    s.symlink_dispatch("mkdir", [])
    s.case("mkdir", ["-m", "invalid", "directory"])
    _filesystem_case(s, ["--context=invalid", "directory"])
    s.case("mkdir", ["-z"])
    s.case("mkdir", ["--context"])

    _filesystem_case(s, ["single"])
    _filesystem_case(s, ["first", "second"])
    _filesystem_case(s, ["-p", "nested/parent/child"])
    _filesystem_case(s, ["-p", "-v", "verbose/parent"])
    _filesystem_case(s, ["--parents", "--verbose", "long-options/path"])
    _filesystem_case(s, ["--verbose", "first", "--mode=750", "second"])
    _filesystem_case(s, ["-m", "755", "--", "-leading-dash"])
    _filesystem_case(s, ["-Z", "context-default"])
    _filesystem_case(s, ["--context", "context-default"])
    _filesystem_case(s, ["-v", "apostrophe'name"])
    _filesystem_case(s, ["-pv", "line\nbreak/child"])
    _filesystem_case(s, ["-m", "700", "private"])
    _filesystem_case(s, ["-m", "u=rwx,g=rx,o=rx", "symbolic"])
    _filesystem_case(s, ["-m", "000", "closed"])
    _filesystem_case(s, ["-m", "+w", "plus-mode"])
    _filesystem_case(s, ["-m", "=rwx", "equal-mode"])
    _filesystem_case(s, ["-m", "a+t", "sticky-mode"])
    _filesystem_case(s, ["-m", "u+s,g+s", "setid-mode"])
    _filesystem_case(s, ["-p", "-m", "700", "mode-parent/child"])
    _filesystem_case(s, ["-p", "existing"], existing_dir="existing")
    _filesystem_case(s, ["existing"], existing_dir="existing")
    _filesystem_case(s, ["-p", "link/child"], existing_symlink="link")
    _filesystem_case(s, ["-p", "block/child"], existing_file=True)
    _filesystem_case(s, ["name with spaces"])
    _filesystem_case(s, ["trailing/slash/"])
    _filesystem_case(s, ["-p", "dot/../normalized/child"])

"""Filesystem parity checks for touch."""

from __future__ import annotations

import os
import stat
import subprocess
import tempfile
from pathlib import Path
from typing import Callable

from support import Suite


def _tree(root: str, *, include_times: bool) -> list[tuple[object, ...]]:
    result: list[tuple[object, ...]] = []
    for current, dirs, files in os.walk(root, followlinks=False):
        dirs.sort()
        files.sort()
        for name in dirs + files:
            path = os.path.join(current, name)
            info = os.lstat(path)
            kind = "dir" if stat.S_ISDIR(info.st_mode) else "file" if stat.S_ISREG(info.st_mode) else "link" if stat.S_ISLNK(info.st_mode) else "other"
            base: tuple[object, ...] = (os.path.relpath(path, root), kind, stat.S_IMODE(info.st_mode), info.st_size)
            if stat.S_ISLNK(info.st_mode):
                base += (os.readlink(path),)
            if include_times:
                base += ((None if stat.S_ISLNK(info.st_mode) else info.st_atime_ns), info.st_mtime_ns)
            result.append(base)
    return result


def _case(
    s: Suite,
    args: list[str],
    *,
    setup: Callable[[str], None] | None = None,
    include_times: bool = True,
) -> None:
    left = tempfile.mkdtemp(prefix="touch-zov-", dir=s.root)
    right = tempfile.mkdtemp(prefix="touch-gnu-", dir=s.root)
    for root in (left, right):
        if setup is not None:
            setup(root)
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    zov = subprocess.run(
        [str(s.zovutils), "touch", *args], cwd=left, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    gnu = subprocess.run(
        ["touch", *args], cwd=right, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5, check=False,
    )
    s.count += 1
    zov_result = (zov.returncode, zov.stdout, zov.stderr, _tree(left, include_times=include_times))
    gnu_result = (gnu.returncode, gnu.stdout, gnu.stderr, _tree(right, include_times=include_times))
    if zov_result != gnu_result:
        raise AssertionError(f"touch filesystem behavior differs for {args!r}:\nzov={zov_result!r}\ngnu={gnu_result!r}")


def _base_files(root: str) -> None:
    target = os.path.join(root, "target")
    reference = os.path.join(root, "reference")
    Path(target).write_bytes(b"target")
    Path(reference).write_bytes(b"reference")
    os.utime(target, ns=(1_000_000_000_123, 1_000_000_000_456))
    os.utime(reference, ns=(1_234_567_890_123, 1_234_567_890_456))
    link = os.path.join(root, "link")
    os.symlink("target", link)
    os.utime(link, ns=(1_111_111_111_123, 1_111_111_111_456), follow_symlinks=False)


def _stdout_case(s: Suite) -> None:
    left = tempfile.mkdtemp(prefix="touch-zov-out-", dir=s.root)
    right = tempfile.mkdtemp(prefix="touch-gnu-out-", dir=s.root)
    for root in (left, right):
        Path(root, "output").write_bytes(b"out")
        os.utime(Path(root, "output"), ns=(1_000_000_000, 1_000_000_000))
    env = s.env.copy()
    env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
    with open(Path(left, "output"), "wb") as output:
        zov = subprocess.run(
            [str(s.zovutils), "touch", "-d", "@1234567890", "-"], cwd=left,
            env=env, stdout=output, stderr=subprocess.PIPE, timeout=5, check=False,
        )
    with open(Path(right, "output"), "wb") as output:
        gnu = subprocess.run(
            ["touch", "-d", "@1234567890", "-"], cwd=right,
            env=env, stdout=output, stderr=subprocess.PIPE, timeout=5, check=False,
        )
    s.count += 1
    zov_result = (zov.returncode, zov.stderr, os.stat(Path(left, "output")).st_mtime_ns)
    gnu_result = (gnu.returncode, gnu.stderr, os.stat(Path(right, "output")).st_mtime_ns)
    if zov_result != gnu_result:
        raise AssertionError(f"touch stdout target behavior differs: zov={zov_result!r} gnu={gnu_result!r}")


def run(s: Suite) -> None:
    s.case("touch", [])
    s.case("touch", ["--unknown"])
    s.case("touch", ["--date"])
    s.case("touch", ["--reference"])
    s.case("touch", ["--time=bad", "file"])
    s.case("touch", ["--help"])
    s.product_version("touch")
    s.symlink_dispatch("touch", [])

    _case(s, ["new"], include_times=False)
    _case(s, ["-c", "missing"], include_times=False)
    _case(s, ["--no-create", "missing"], include_times=False)
    _case(s, ["-d", "@1234567890.125", "target"], setup=_base_files)
    _case(s, ["-d", "@-0.25", "target"], setup=_base_files)
    _case(s, ["-d", "2001-02-03", "target"], setup=_base_files)
    _case(s, ["-d", "2001-02-03 04:05", "target"], setup=_base_files)
    _case(s, ["-d", "2001-02-03T04:05:06.125Z", "target"], setup=_base_files)
    _case(s, ["-d", "2001-02-03 04:05:06 UTC", "target"], setup=_base_files)
    _case(s, ["-t", "200102030405.06", "target"], setup=_base_files)
    _case(s, ["-t200102030405", "--", "target"], setup=_base_files)
    _case(s, ["-t", "0102030405", "target"], setup=_base_files)
    _case(s, ["--dat=@1234567890.125", "target"], setup=_base_files)
    _case(s, ["--time=modify", "-d", "@1234567890", "target"], setup=_base_files)
    _case(s, ["--time=access", "--date=@1234567890", "target"], setup=_base_files)
    _case(s, ["-a", "-m", "-d", "@1234567890", "target"], setup=_base_files)
    _case(s, ["-d", "@1234567890", "-r", "reference", "target"], setup=_base_files)
    _case(s, ["-d", "now", "target"], setup=_base_files, include_times=False)
    _case(s, ["-a", "-d", "@1234567890", "target"], setup=_base_files)
    _case(s, ["-r", "reference", "target"], setup=_base_files)
    _case(s, ["-h", "-d", "@1234567890", "link"], setup=_base_files)
    _case(s, ["-h", "missing"], include_times=False)
    _case(s, ["-"], include_times=False)
    _stdout_case(s)
    _case(s, ["--", "-leading-dash"], include_times=False)
    _case(s, ["-d", "not-a-date", "target"], setup=_base_files)
    _case(s, ["-t", "invalid", "target"], setup=_base_files)

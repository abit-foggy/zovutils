#!/usr/bin/env python3
"""Fixture-scoped Linux checks requiring root or CAP_MKNOD."""
from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

from support import ParityFailure, Suite, require_executables


def _copy_runtime(root: Path, executable: Path) -> None:
    destination = root / "bin" / "true"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(executable.resolve(), destination)
    result = subprocess.run(["ldd", str(executable)], stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, timeout=10, check=False)
    if result.returncode != 0:
        raise ParityFailure(f"ldd could not inspect {executable}: {result.stderr}")
    libraries: set[Path] = set()
    for line in result.stdout.splitlines():
        match = re.search(r"=>\s+(/[^ ]+)|^\s*(/[^ ]+)\s+\(", line)
        if match:
            libraries.add(Path(match.group(1) or match.group(2)))
    for library in libraries:
        target = root / str(library).lstrip("/")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(library.resolve(), target)


def run(zovutils: Path, gnu_dir: Path) -> int:
    if sys.platform != "linux" or os.geteuid() != 0:
        raise ParityFailure("privileged parity must run as root on Linux")
    suite = Suite(zovutils.resolve(), gnu_dir.resolve())
    try:
        require_executables(suite)
        empty = lambda _root: None
        for args in (("char-device", "c", "1", "3"), ("block-device", "b", "7", "0")):
            suite.case_filesystem("mknod", list(args), setup=empty, expected_status=0)
        suite.case_filesystem("mknod", ["pipe", "p"], setup=empty, expected_status=0)

        true_binary = Path("/bin/true")
        if not true_binary.exists():
            true_binary = Path("/usr/bin/true")
        suite.case_filesystem(
            "chroot", [".", "/bin/true"],
            setup=lambda root: _copy_runtime(root, true_binary),
            expected_status=0,
        )

        enforce = Path("/sys/fs/selinux/enforce")
        if enforce.exists() and enforce.read_text().strip() == "1":
            probe = suite.root / "selinux-probe"
            probe.write_bytes(b"context probe")
            try:
                context = os.getxattr(probe, "security.selinux").rstrip(b"\0").decode()
            except OSError as exc:
                raise ParityFailure(f"SELinux is enforcing but its file context is unavailable: {exc}") from exc
            suite.case_filesystem(
                "chcon", [context, "target"],
                setup=lambda root: (root / "target").write_bytes(b"target"),
                expected_status=0,
            )
            process_context = Path("/proc/self/attr/current").read_text().strip()
            suite.case("runcon", [process_context, "/bin/true"])
        else:
            print("SELinux context success cases skipped: runner policy is not enforcing")
        print(f"privileged Linux parity passed: {suite.count} cases")
        return 0
    finally:
        suite.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zovutils", type=Path, default=Path("target/zovutils"))
    parser.add_argument("--gnu-dir", type=Path, default=Path(os.environ.get("GNU_COREUTILS_DIR", "/usr/bin")))
    args = parser.parse_args()
    try:
        return run(args.zovutils, args.gnu_dir)
    except (ParityFailure, OSError, AssertionError) as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())

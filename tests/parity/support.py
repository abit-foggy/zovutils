"""Shared byte-for-byte GNU Coreutils parity helpers."""

from __future__ import annotations

import difflib
import os
import subprocess
import tempfile
from pathlib import Path


class ParityFailure(AssertionError):
    pass


class Suite:
    def __init__(self, zovutils: Path, gnu_dir: Path) -> None:
        self.zovutils = zovutils
        self.gnu_dir = gnu_dir
        self.tmp = tempfile.TemporaryDirectory(prefix="zovutils-parity-")
        self.root = Path(self.tmp.name)
        self.env = os.environ.copy()
        self.env.update({"LC_ALL": "C", "LANG": "C", "TZ": "UTC0"})
        self.count = 0

    def close(self) -> None:
        self.tmp.cleanup()

    def path(self, name: str) -> str:
        return str(self.root / name)

    def fixture(self, name: str, data: bytes) -> str:
        path = self.root / name
        path.write_bytes(data)
        return str(path)

    def case(self, utility: str, args: list[str], *, stdin: bytes = b"") -> None:
        self.count += 1
        zov = self._run([str(self.zovutils), utility, *args], stdin)
        gnu = self._run([str(self.gnu_dir / utility), *args], stdin)
        if zov != gnu:
            label = f"{utility} {' '.join(args)}".rstrip()
            details = [f"parity failed: {label}", f"statuses: zov={zov[0]} gnu={gnu[0]}"]
            for name, left, right in (
                ("stdout", gnu[1], zov[1]),
                ("stderr", gnu[2], zov[2]),
            ):
                if left != right:
                    details.append(f"{name} differs (GNU={left!r}, zov={right!r})")
                    if b"\0" not in left + right:
                        diff = difflib.unified_diff(
                            left.decode("utf-8", "replace").splitlines(True),
                            right.decode("utf-8", "replace").splitlines(True),
                            fromfile="GNU",
                            tofile="zov",
                        )
                        details.extend(line.rstrip("\n") for line in diff)
            raise ParityFailure("\n".join(details))

    def _run(self, command: list[str], stdin: bytes) -> tuple[int, bytes, bytes]:
        try:
            result = subprocess.run(
                command,
                input=stdin,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=self.env,
                timeout=15,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise ParityFailure(f"timed out after 15 seconds: {command!r}") from exc
        return result.returncode, result.stdout, result.stderr

    def oracle_version(self) -> str:
        result = subprocess.run(
            [str(self.gnu_dir / "cat"), "--version"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env=self.env,
            timeout=5,
            check=False,
        )
        return result.stdout.decode("utf-8", "replace").splitlines()[0]


def require_executables(suite: Suite) -> None:
    if not suite.zovutils.is_file() or not os.access(suite.zovutils, os.X_OK):
        raise ParityFailure(f"missing zovutils executable: {suite.zovutils}")
    for utility in ("cat", "head", "tail", "wc"):
        candidate = suite.gnu_dir / utility
        if not candidate.is_file() or not os.access(candidate, os.X_OK):
            raise ParityFailure(f"missing GNU utility: {candidate}")
    version = suite.oracle_version()
    if "GNU coreutils) 9.11" not in version:
        raise ParityFailure(f"expected GNU Coreutils 9.11, found: {version}")

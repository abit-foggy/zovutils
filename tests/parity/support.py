"""Shared byte-for-byte GNU Coreutils parity helpers."""

from __future__ import annotations

import difflib
import os
import re
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
        self.env.update({"LC_ALL": "C", "LANG": "C", "TZ": "UTC0", "TERM": "dumb"})
        self.count = 0

    def close(self) -> None:
        self.tmp.cleanup()

    def path(self, name: str) -> str:
        return str(self.root / name)

    def fixture(self, name: str, data: bytes) -> str:
        path = self.root / name
        path.write_bytes(data)
        return str(path)

    def case(
        self,
        utility: str,
        args: list[str],
        *,
        stdin: bytes = b"",
        extra_env: dict[str, str] | None = None,
    ) -> None:
        self.count += 1
        gnu_env = self.env.copy()
        if extra_env:
            gnu_env.update(extra_env)
        zov_env = self.env.copy()
        if extra_env:
            zov_env.update(extra_env)
        gnu_env["PATH"] = str(self.gnu_dir) + os.pathsep + gnu_env.get("PATH", "")
        zov = self._run([str(self.zovutils), utility, *args], stdin, env=zov_env)
        gnu = self._run([utility, *args], stdin, env=gnu_env)
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

    def _run(
        self, command: list[str], stdin: bytes, *, env: dict[str, str] | None = None
    ) -> tuple[int, bytes, bytes]:
        try:
            result = subprocess.run(
                command,
                input=stdin,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env or self.env,
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

    def product_version(self, utility: str) -> None:
        manifest = Path(__file__).resolve().parents[2] / "zov.toml"
        match = re.search(r'^version = "([^"]+)"$', manifest.read_text(), re.MULTILINE)
        if match is None:
            raise ParityFailure("zov.toml does not declare a package version")
        result = self._run([str(self.zovutils), utility, "--version"], b"")
        expected = f"{utility} (zovutils) {match.group(1)}\n".encode()
        if result != (0, expected, b""):
            raise ParityFailure(f"{utility} --version does not report package version: {result!r}")

    def write_failure(self, utility: str, args: list[str]) -> None:
        """Compare output error handling where Linux provides /dev/full."""
        sink = Path("/dev/full")
        if not sink.exists():
            return
        gnu_env = self.env.copy()
        gnu_env["PATH"] = str(self.gnu_dir) + os.pathsep + gnu_env.get("PATH", "")
        results = []
        for command, env in (
            ([str(self.zovutils), utility, *args], self.env),
            ([utility, *args], gnu_env),
        ):
            with sink.open("wb") as stdout:
                proc = subprocess.run(
                    command,
                    stdout=stdout,
                    stderr=subprocess.PIPE,
                    env=env,
                    timeout=15,
                    check=False,
                )
            results.append((proc.returncode, proc.stderr))
        self.count += 1
        if results[0] != results[1]:
            raise ParityFailure(f"{utility} output-write failure differs: {results!r}")

    def permission_denied_case(self, utility: str, args: list[str], path: str) -> None:
        """Compare an unreadable file and restore it for temporary-dir cleanup."""
        os.chmod(path, 0)
        try:
            if not os.access(path, os.R_OK):
                self.case(utility, args)
        finally:
            os.chmod(path, 0o600)

    def broken_pipe_case(self, utility: str, args: list[str]) -> None:
        """Compare SIGPIPE behavior after a downstream reader exits."""
        results = []
        for command in (
            [str(self.zovutils), utility, *args],
            [str(self.gnu_dir / utility), *args],
        ):
            proc = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=self.env,
            )
            assert proc.stdout is not None and proc.stderr is not None
            proc.stdout.close()
            stderr = proc.stderr.read()
            try:
                status = proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
                raise ParityFailure(f"{utility} did not stop after a broken pipe")
            results.append((status, stderr))
        self.count += 1
        if results[0] != results[1]:
            raise ParityFailure(f"{utility} broken-pipe behavior differs: {results!r}")

    def symlink_dispatch(self, utility: str, args: list[str]) -> None:
        alias = self.root / utility
        alias.symlink_to(self.zovutils)
        zov = self._run([str(alias), *args], b"")
        gnu_env = self.env.copy()
        gnu_env["PATH"] = str(self.gnu_dir) + os.pathsep + gnu_env.get("PATH", "")
        gnu = self._run([utility, *args], b"", env=gnu_env)
        self.count += 1
        if zov != gnu:
            raise ParityFailure(f"{utility} argv[0] symlink dispatch differs: {zov!r} != {gnu!r}")


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

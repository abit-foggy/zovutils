"""Shared byte-for-byte GNU Coreutils parity helpers."""

from __future__ import annotations

import difflib
import os
import pty
import re
import stat
import select
import signal
import subprocess
import sys
import tempfile
import tty
from pathlib import Path
from typing import Callable


class ParityFailure(AssertionError):
    pass


def _ignore_sigpipe() -> None:
    signal.signal(signal.SIGPIPE, signal.SIG_IGN)


class Suite:
    def __init__(self, zovutils: Path, gnu_dir: Path) -> None:
        self.zovutils = zovutils
        self.gnu_dir = gnu_dir
        self.tmp = tempfile.TemporaryDirectory(prefix="zovutils-parity-")
        self.root = Path(self.tmp.name)
        self.env = os.environ.copy()
        for variable in ("POSIXLY_CORRECT", "QUOTING_STYLE"):
            self.env.pop(variable, None)
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

    def byte_name_fixture(self, name: str, data: bytes) -> str:
        # macOS filesystems reject non-UTF-8 names, but the invalid-byte argv
        # behavior can still be compared using a path that does not exist.
        if sys.platform == "darwin":
            return self.path(name)
        return self.fixture(name, data)

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
        if utility == "stdbuf":
            # stdbuf must launch the same command under both implementations;
            # otherwise host binaries can differ in architecture (notably
            # arm64e versus the arm64 GNU oracle build on macOS).
            zov_env["PATH"] = str(self.gnu_dir) + os.pathsep + zov_env.get("PATH", "")
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
        self,
        command: list[str],
        stdin: bytes,
        *,
        env: dict[str, str] | None = None,
        cwd: Path | None = None,
    ) -> tuple[int, bytes, bytes]:
        try:
            result = subprocess.run(
                command,
                input=stdin,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env or self.env,
                cwd=cwd,
                timeout=15,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise ParityFailure(f"timed out after 15 seconds: {command!r}") from exc
        return result.returncode, result.stdout, result.stderr

    @staticmethod
    def _filesystem_snapshot(root: Path, *, compare_mtimes: bool = False) -> list[tuple[object, ...]]:
        snapshot: list[tuple[object, ...]] = []
        inode_groups: dict[tuple[int, int], int] = {}
        for current, dirs, files in os.walk(root, followlinks=False):
            dirs.sort()
            files.sort()
            for name in dirs + files:
                path = Path(current, name)
                info = path.lstat()
                mode = stat.S_IMODE(info.st_mode)
                if stat.S_ISDIR(info.st_mode):
                    kind, detail = "directory", b""
                elif stat.S_ISREG(info.st_mode):
                    kind = "file"
                    identity = (info.st_dev, info.st_ino)
                    group = inode_groups.setdefault(identity, len(inode_groups))
                    detail = (path.read_bytes(), group if info.st_nlink > 1 else None)
                elif stat.S_ISLNK(info.st_mode):
                    kind, detail = "symlink", os.readlink(path)
                elif stat.S_ISFIFO(info.st_mode):
                    kind, detail = "fifo", b""
                elif stat.S_ISCHR(info.st_mode) or stat.S_ISBLK(info.st_mode):
                    kind = "character device" if stat.S_ISCHR(info.st_mode) else "block device"
                    detail = (os.major(info.st_rdev), os.minor(info.st_rdev))
                else:
                    kind, detail = "special", b""
                xattrs: tuple[tuple[str, bytes], ...] = ()
                if not stat.S_ISLNK(info.st_mode) and hasattr(os, "listxattr"):
                    try:
                        xattrs = tuple((key, os.getxattr(path, key, follow_symlinks=False))
                                       for key in sorted(os.listxattr(path, follow_symlinks=False)))
                    except OSError:
                        xattrs = ()
                row: tuple[object, ...] = (path.relative_to(root).as_posix(), kind, mode, detail, xattrs)
                if compare_mtimes:
                    row += (info.st_mtime_ns,)
                snapshot.append(row)
        return snapshot

    def case_filesystem(
        self,
        utility: str,
        args: list[str],
        *,
        setup: Callable[[Path], None],
        stdin: bytes = b"",
        extra_env: dict[str, str] | None = None,
        compare_mtimes: bool = False,
        expected_status: int | None = None,
    ) -> tuple[tuple[int, bytes, bytes], tuple[int, bytes, bytes]]:
        """Compare command output and fixture-tree effects in isolated roots."""
        with tempfile.TemporaryDirectory(prefix=f"{utility}-zov-", dir=self.root) as zdir, \
             tempfile.TemporaryDirectory(prefix=f"{utility}-gnu-", dir=self.root) as gdir:
            zroot, groot = Path(zdir), Path(gdir)
            setup(zroot)
            setup(groot)
            zenv = self.env.copy()
            genv = self.env.copy()
            if extra_env:
                zenv.update(extra_env)
                genv.update(extra_env)
            genv["PATH"] = str(self.gnu_dir) + os.pathsep + genv.get("PATH", "")
            zov = self._run([str(self.zovutils), utility, *args], stdin, env=zenv, cwd=zroot)
            gnu = self._run([utility, *args], stdin, env=genv, cwd=groot)
            zsnapshot = self._filesystem_snapshot(zroot, compare_mtimes=compare_mtimes)
            gsnapshot = self._filesystem_snapshot(groot, compare_mtimes=compare_mtimes)
            self.count += 1
            if zov != gnu or zsnapshot != gsnapshot:
                raise ParityFailure(
                    f"{utility} filesystem parity failed for {args!r}: "
                    f"zov={zov!r}, GNU={gnu!r}, zov tree={zsnapshot!r}, GNU tree={gsnapshot!r}"
                )
            if expected_status is not None and (zov[0] != expected_status or gnu[0] != expected_status):
                raise ParityFailure(
                    f"{utility} did not return expected status {expected_status} for {args!r}: "
                    f"zov={zov[0]}, GNU={gnu[0]}"
                )
            return zov, gnu

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

    def product_version(self, utility: str, *, status: int = 0) -> None:
        manifest = Path(__file__).resolve().parents[2] / "zov.toml"
        match = re.search(r'^version = "([^"]+)"$', manifest.read_text(), re.MULTILINE)
        if match is None:
            raise ParityFailure("zov.toml does not declare a package version")
        result = self._run([str(self.zovutils), utility, "--version"], b"")
        expected = f"{utility} (zovutils) {match.group(1)}\n".encode()
        if result != (status, expected, b""):
            raise ParityFailure(f"{utility} --version does not report package version: {result!r}")

    def write_failure(self, utility: str, args: list[str]) -> None:
        """Compare output errors on /dev/full and a closed pipe."""
        sink = Path("/dev/full")
        gnu_env = self.env.copy()
        gnu_env["PATH"] = str(self.gnu_dir) + os.pathsep + gnu_env.get("PATH", "")
        commands = (
            ([str(self.zovutils), utility, *args], self.env),
            ([utility, *args], gnu_env),
        )
        methods: list[list[tuple[int, bytes]]] = []
        if sink.exists():
            results = []
            for command, env in commands:
                with sink.open("wb") as stdout:
                    proc = subprocess.run(
                        command,
                        stdin=subprocess.DEVNULL,
                        stdout=stdout,
                        stderr=subprocess.PIPE,
                        env=env,
                        timeout=15,
                        check=False,
                    )
                results.append((proc.returncode, proc.stderr))
            methods.append(results)

        results = []
        read_fd, write_fd = os.pipe()
        os.close(read_fd)
        try:
            for command, env in commands:
                proc = subprocess.run(
                    command,
                    stdin=subprocess.DEVNULL,
                    stdout=write_fd,
                    stderr=subprocess.PIPE,
                    env=env,
                    timeout=15,
                    check=False,
                    preexec_fn=_ignore_sigpipe,
                )
                results.append((proc.returncode, proc.stderr))
        finally:
            os.close(write_fd)
        methods.append(results)

        for results in methods:
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

    def prefix_case(
        self, utility: str, args: list[str], *, size: int = 4096, symlink: bool = False
    ) -> None:
        """Compare a fixed output prefix from commands that do not terminate."""
        gnu_env = self.env.copy()
        gnu_env["PATH"] = str(self.gnu_dir) + os.pathsep + gnu_env.get("PATH", "")
        zov_command = [str(self.zovutils), utility, *args]
        if symlink:
            alias = self.root / utility
            alias.symlink_to(self.zovutils)
            zov_command = [str(alias), *args]
        results = []
        for command, env in (
            (zov_command, self.env),
            ([utility, *args], gnu_env),
        ):
            proc = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
            )
            assert proc.stdout is not None
            output = bytearray()
            try:
                while len(output) < size:
                    ready, _, _ = select.select([proc.stdout], [], [], 10)
                    if not ready:
                        raise ParityFailure(f"{utility} produced fewer than {size} bytes in 10 seconds")
                    chunk = os.read(proc.stdout.fileno(), size - len(output))
                    if not chunk:
                        break
                    output.extend(chunk)
            finally:
                proc.kill()
                proc.wait(timeout=5)
                proc.stdout.close()
                if proc.stderr is not None:
                    proc.stderr.close()
            results.append(bytes(output))
        self.count += 1
        if results[0] != results[1]:
            raise ParityFailure(f"{utility} output prefix differs: {results[0]!r} != {results[1]!r}")

    def interrupted_read_case(
        self,
        utility: str,
        args: list[str],
        *,
        extra_env: dict[str, str] | None = None,
    ) -> None:
        """Compare how the utility reports EINTR from its first stdin read."""
        if sys.platform != "linux":
            return
        source = self.root / "interrupt-read.c"
        library = self.root / "interrupt-read.so"
        source.write_text(
            """
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
static ssize_t (*next_read)(int, void *, size_t);
static int injected;
ssize_t read(int fd, void *buffer, size_t count) {
    if (!next_read) next_read = dlsym(RTLD_NEXT, "read");
    if (fd == STDIN_FILENO && !injected++) { errno = EINTR; return -1; }
    return next_read(fd, buffer, count);
}
"""
        )
        subprocess.run(
            ["cc", "-shared", "-fPIC", str(source), "-ldl", "-o", str(library)],
            check=True,
            timeout=15,
        )
        env = self.env.copy()
        if extra_env:
            env.update(extra_env)
        env["LD_PRELOAD"] = str(library)
        results = [
            self._run([str(self.zovutils), utility, *args], b"interrupted input\n", env=env),
            self._run([str(self.gnu_dir / utility), *args], b"interrupted input\n", env=env),
        ]
        self.count += 1
        if results[0] != results[1]:
            raise ParityFailure(f"{utility} interrupted-read behavior differs: {results!r}")

    def read_error_after_partial_case(self, utility: str, args: list[str]) -> None:
        """Compare a read error after stdin has already supplied one chunk."""
        if sys.platform != "linux":
            return
        source = self.root / f"{utility}-partial-error-read.c"
        library = self.root / f"{utility}-partial-error-read.so"
        source.write_text(
            """
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
static ssize_t (*next_read)(int, void *, size_t);
static int injected;
ssize_t read(int fd, void *buffer, size_t count) {
    if (!next_read) next_read = dlsym(RTLD_NEXT, "read");
    if (fd == STDIN_FILENO && injected++ > 0) {
        errno = EIO;
        return -1;
    }
    return next_read(fd, buffer, count);
}
"""
        )
        subprocess.run(
            ["cc", "-shared", "-fPIC", str(source), "-ldl", "-o", str(library)],
            check=True,
            timeout=15,
        )
        env = self.env.copy()
        env["LD_PRELOAD"] = str(library)
        payload = b"one two\nsecond line\n"
        results = [
            self._run([str(self.zovutils), utility, *args], payload, env=env),
            self._run([str(self.gnu_dir / utility), *args], payload, env=env),
        ]
        self.count += 1
        if results[0] != results[1]:
            raise ParityFailure(f"{utility} post-input read-error behavior differs: {results!r}")

    def pty_read_error_case(self, utility: str, args: list[str]) -> None:
        """Compare partial input followed by Linux PTY-master EIO."""
        if not sys.platform.startswith("linux"):
            return
        payload = b"one two\nsecond line\n"
        results = []
        for command in (
            [str(self.zovutils), utility, *args],
            [str(self.gnu_dir / utility), *args],
        ):
            master, slave = pty.openpty()
            tty.setraw(slave)
            os.write(slave, payload)
            try:
                proc = subprocess.Popen(
                    command,
                    stdin=master,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    env=self.env,
                )
            finally:
                os.close(master)
                os.close(slave)
            try:
                stdout, stderr = proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                stdout, stderr = proc.communicate()
                raise ParityFailure(f"{utility} did not stop after PTY read error")
            results.append((proc.returncode, stdout, stderr))
        self.count += 1
        if results[0] != results[1]:
            raise ParityFailure(f"{utility} PTY read-error behavior differs: {results!r}")

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

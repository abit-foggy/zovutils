"""Parity cases for tail, including bounded follow-mode checks."""

from __future__ import annotations

import subprocess
import time

from support import ParityFailure, Suite


def _follow(s: Suite, mode: str, *, pid_bound: bool = False) -> None:
    path = s.fixture(f"tail-follow-{mode}", b"initial\n")
    zov_out = open(s.path(f"{mode}-zov.out"), "wb")
    gnu_out = open(s.path(f"{mode}-gnu.out"), "wb")
    args = ["-f", "-s", "0.1"] if mode.startswith("descriptor") else ["--follow=name", "--sleep-interval=0.1"]
    child: subprocess.Popen[bytes] | None = None
    zov: subprocess.Popen[bytes] | None = None
    gnu: subprocess.Popen[bytes] | None = None
    try:
        if pid_bound:
            child = subprocess.Popen(["python3", "-c", "import time; time.sleep(0.7)"])
            args.extend(["--pid", str(child.pid), "-s", "0.1"])
        zov = subprocess.Popen(
            [str(s.zovutils), "tail", *args, path], stdout=zov_out, stderr=subprocess.PIPE, env=s.env
        )
        gnu = subprocess.Popen(
            [str(s.gnu_dir / "tail"), *args, path], stdout=gnu_out, stderr=subprocess.PIPE, env=s.env
        )
        time.sleep(0.3)
        with open(path, "ab") as fixture:
            fixture.write(b"appended\n")
        if not pid_bound:
            time.sleep(0.4)
            zov.terminate()
            gnu.terminate()
        elif child is not None:
            child.wait(timeout=3)
        for process in (zov, gnu):
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                raise ParityFailure(f"tail {mode} follow did not stop within timeout")
        zov_out.flush()
        gnu_out.flush()
        zov_data = open(s.path(f"{mode}-zov.out"), "rb").read()
        gnu_data = open(s.path(f"{mode}-gnu.out"), "rb").read()
        s.count += 1
        if zov_data != gnu_data:
            raise ParityFailure(f"tail {mode} follow stdout differs: zov={zov_data!r}, gnu={gnu_data!r}")
    finally:
        for process in (zov, gnu, child):
            if process is not None and process.poll() is None:
                process.kill()
                process.wait()
        zov_out.close()
        gnu_out.close()


def run(s: Suite) -> None:
    empty = s.fixture("tail-empty", b"")
    lines = s.fixture("tail-lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("tail-no-final-newline", b"a\nb\nc")
    binary = s.fixture("tail-binary", b"a\0b\n\xffz")
    large = s.fixture("tail-large", b"row\n" * 25000)
    missing = s.path("tail-missing")

    options = (
        [],
        ["-n", "2"],
        ["-n2"],
        ["-n", "-2"],
        ["-n", "0"],
        ["-n", "+2"],
        ["-n", "+0"],
        ["-c", "4"],
        ["-c4"],
        ["-c", "+3"],
        ["-c", "-2"],
        ["-c", "0"],
        ["--lines=2"],
        ["--bytes=5"],
        ["-z", "-n", "2"],
        ["-q", "-n", "1"],
        ["-v", "-n", "1"],
        ["-n", "100000"],
    )
    fixtures = (lines, no_final_newline, binary, large)
    for opts in options:
        for fixture in fixtures:
            s.case("tail", [*opts, fixture])

    for args in (
        ["-n", "1", lines, binary],
        ["-q", "-n", "1", lines, binary],
        ["-v", "-n", "1", lines, binary],
        ["-n", "1", lines, missing],
        ["--", "-n"],
    ):
        s.case("tail", args)
    s.case("tail", ["-n", "2"], stdin=b"one\ntwo\nthree\n")
    s.case("tail", ["-c", "4"], stdin=b"a\0b\n\xff")
    _follow(s, "descriptor")
    _follow(s, "name")
    _follow(s, "descriptor-pid", pid_bound=True)

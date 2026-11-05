"""Parity cases for tail, including bounded follow-mode checks."""

from __future__ import annotations

import subprocess
import time
import os
from pathlib import Path

from support import ParityFailure, Suite


def _follow(
    s: Suite,
    mode: str,
    *,
    pid_bound: bool = False,
    rotation: bool = False,
    initial_missing: bool = False,
    truncation: bool = False,
    multi_file: bool = False,
    quiet: bool = False,
    verbose: bool = False,
) -> None:
    path = s.path(f"tail-follow-{mode}")
    if not initial_missing:
        s.fixture(f"tail-follow-{mode}", b"initial\n")
    paths = [path]
    if multi_file:
        second_path = s.fixture(f"tail-follow-{mode}-second", b"second\n")
        paths.append(second_path)
    zov_out = open(s.path(f"{mode}-zov.out"), "wb")
    gnu_out = open(s.path(f"{mode}-gnu.out"), "wb")
    zov_err_file = open(s.path(f"{mode}-zov.err"), "wb")
    gnu_err_file = open(s.path(f"{mode}-gnu.err"), "wb")
    if mode == "legacy-follow":
        args = ["-2f"]
    elif mode == "descriptor" or mode.startswith("descriptor-"):
        args = ["-f", "-s", "0.1"]
    elif mode == "descriptor-long":
        args = ["--follow=descriptor", "--sleep-interval=0.1"]
    elif mode.startswith("F-"):
        args = ["-F", "-s", "0.1"]
    elif mode == "name-abbrev":
        args = ["--fo=name", "--sleep-interval=0.1", "--max-unchanged-stats=1"]
    else:
        args = ["--follow=name", "--sleep-interval=0.1", "--max-unchanged-stats=1"]
    if quiet:
        args.append("--quiet")
    if verbose:
        args.append("--verbose")
    if initial_missing:
        args.append("--retry")
    child: subprocess.Popen[bytes] | None = None
    zov: subprocess.Popen[bytes] | None = None
    gnu: subprocess.Popen[bytes] | None = None
    try:
        if pid_bound:
            child = subprocess.Popen(["python3", "-c", "import time; time.sleep(0.7)"])
            args.extend(["--pid", str(child.pid), "-s", "0.1"])
        zov = subprocess.Popen(
            [str(s.zovutils), "tail", *args, *paths], stdout=zov_out, stderr=zov_err_file, env=s.env
        )
        gnu = subprocess.Popen(
            [str(s.gnu_dir / "tail"), *args, *paths], stdout=gnu_out, stderr=gnu_err_file, env=s.env
        )
        has_headers = verbose or (multi_file and not quiet)
        if multi_file and has_headers:
            initial_output = f"==> {path} <==\ninitial\n\n==> {paths[-1]} <==\nsecond\n".encode()
        elif multi_file:
            initial_output = b"initial\nsecond\n"
        elif verbose:
            initial_output = f"==> {path} <==\ninitial\n".encode()
        else:
            initial_output = b"initial\n"
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            zov_out.flush()
            gnu_out.flush()
            zov_err_file.flush()
            gnu_err_file.flush()
            if initial_missing:
                ready = all(Path(s.path(f"{mode}-{who}.err")).read_bytes().startswith(b"tail: cannot open") for who in ("zov", "gnu"))
            else:
                ready = all(Path(s.path(f"{mode}-{who}.out")).read_bytes().startswith(initial_output) for who in ("zov", "gnu"))
            if ready:
                break
            time.sleep(0.02)
        else:
            raise ParityFailure(f"tail {mode} follow did not reach its initial state")
        if initial_missing:
            with open(path, "wb") as fixture:
                fixture.write(b"appeared\n")
            time.sleep(0.3)
        elif rotation:
            replacement = path + ".replacement"
            with open(replacement, "wb") as fixture:
                fixture.write(b"invoked\n")
            os.replace(replacement, path)
            time.sleep(0.3)
        if truncation:
            with open(path, "wb") as fixture:
                fixture.write(b"after\n")
        else:
            with open(path, "ab") as fixture:
                fixture.write(b"appended\n")
        if multi_file:
            time.sleep(0.25)
            with open(paths[1], "ab") as fixture:
                fixture.write(b"appended-second\n")
        if not pid_bound:
            time.sleep(1.2 if mode == "legacy-follow" else 0.4)
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
        zov_err_file.flush()
        gnu_err_file.flush()
        zov_data = open(s.path(f"{mode}-zov.out"), "rb").read()
        gnu_data = open(s.path(f"{mode}-gnu.out"), "rb").read()
        zov_err = Path(s.path(f"{mode}-zov.err")).read_bytes()
        gnu_err = Path(s.path(f"{mode}-gnu.err")).read_bytes()
        zov_status = zov.returncode
        gnu_status = gnu.returncode
        s.count += 1
        if (zov_data, zov_err, zov_status) != (gnu_data, gnu_err, gnu_status):
            raise ParityFailure(
                f"tail {mode} follow differs: zov={(zov_data, zov_err, zov_status)!r}, "
                f"gnu={(gnu_data, gnu_err, gnu_status)!r}"
            )
    finally:
        for process in (zov, gnu, child):
            if process is not None and process.poll() is None:
                process.kill()
                process.wait()
        zov_out.close()
        gnu_out.close()
        zov_err_file.close()
        gnu_err_file.close()


def _debug_report(s: Suite) -> None:
    path = s.fixture("tail-debug", b"initial\n")
    child = subprocess.Popen(["python3", "-c", "import time; time.sleep(0.3)"])
    proc = subprocess.Popen(
        [str(s.zovutils), "tail", "--debug", "-f", "-s", "0.05", "--pid", str(child.pid), path],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=s.env,
    )
    child.wait(timeout=3)
    stdout, stderr = proc.communicate(timeout=3)
    if proc.returncode != 0 or stdout != b"initial\n" or stderr != b"tail: using polling method\n":
        raise ParityFailure(f"tail --debug polling report differs: {(proc.returncode, stdout, stderr)!r}")


def _follow_write_failure(s: Suite) -> None:
    sink = Path("/dev/full")
    if not sink.exists():
        return
    path = s.fixture("tail-follow-write-error", b"initial\n")
    children: list[subprocess.Popen[bytes]] = []
    processes: list[subprocess.Popen[bytes]] = []
    sinks = []
    try:
        for command in (
            [str(s.zovutils), "tail"],
            [str(s.gnu_dir / "tail")],
        ):
            child = subprocess.Popen(["python3", "-c", "import time; time.sleep(0.3)"])
            children.append(child)
            command.extend(["--follow=descriptor", "--pid", str(child.pid), "-s", "0.05", path])
            output = sink.open("wb")
            sinks.append(output)
            processes.append(
                subprocess.Popen(command, stdout=output, stderr=subprocess.PIPE, env=s.env)
            )
        for child in children:
            child.wait(timeout=3)
        results = [proc.communicate(timeout=3) + (proc.returncode,) for proc in processes]
        s.count += 1
        if results[0] != results[1]:
            raise ParityFailure(f"tail follow output-write failure differs: {results!r}")
    finally:
        for proc in processes:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
        for child in children:
            if child.poll() is None:
                child.kill()
                child.wait()
        for output in sinks:
            output.close()


def run(s: Suite) -> None:
    s.product_version("tail")
    empty = s.fixture("tail-empty", b"")
    lines = s.fixture("tail-lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("tail-no-final-newline", b"a\nb\nc")
    binary = s.fixture("tail-binary", b"a\0b\n\xffz")
    large = s.fixture("tail-large", b"row\n" * 25000)
    missing = s.path("tail-missing")
    unusual_name = s.fixture("tail-line\nbreak", b"name with newline\n")
    invalid_byte_name = s.fixture("tail-invalid-\udcff", b"name with invalid byte\n")
    missing_unusual = s.path("tail-missing-line\nbreak")
    missing_quote_control = s.path("tail-missing-'line\nbreak")
    unreadable = s.fixture("tail-unreadable", b"unreadable\n")
    s.write_failure("tail", [lines])
    s.write_failure("tail", ["--help"])
    s.write_failure("tail", ["--version"])
    s.broken_pipe_case("tail", ["-c", "100000", large])
    s.symlink_dispatch("tail", ["-n", "1", lines])

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
        ["-n", "+2", "-c1"],
        ["-c1", "-n", "+2"],
        ["-n", "+2", "-n", "2"],
        ["--lines=2"],
        ["--bytes=5"],
        ["-z", "-n", "2"],
        ["-q", "-n", "1"],
        ["-v", "-n", "1"],
        ["-n", "100000"],
        ["+2"],
        ["+0"],
        ["+2c"],
        ["+2b"],
        ["+2kb"],
        ["+2l"],
        ["-2c"],
        ["-2b"],
        ["-2kb"],
        ["-2K"],
        ["-2l"],
        ["+2K"],
    )
    fixtures = (empty, lines, no_final_newline, binary, large)
    for opts in options:
        for fixture in fixtures:
            s.case("tail", [*opts, fixture])

    for value in ("0", "1", "2", "-1", "-2", "+0", "+1", "+2", "1K", "1b", "1Q"):
        for flag in ("-n", "-c"):
            for fixture in (empty, lines, no_final_newline, binary):
                s.case("tail", [flag, value, fixture])
        for flag in ("--lines=", "--bytes="):
            s.case("tail", [flag + value, lines])

    for option in ("--lines", "--bytes"):
        for length in range(3, len(option) + 1):
            s.case("tail", [option[:length] + "=2", lines])
    for option, value in (("--sleep-interval", "0.001"), ("--max-unchanged-stats", "1")):
        for length in range(3, len(option) + 1):
            s.case("tail", [option[:length] + "=" + value, lines])
    for option in ("--quiet", "--silent", "--verbose", "--zero-terminated", "--retry", "--debug"):
        for length in range(3, len(option) + 1):
            s.case("tail", [option[:length], "-n", "1", lines])

    for value in (
        "1b", "1kB", "1K", "1KiB", "1MB", "1M", "1GB", "1G", "1TB", "1T",
        "1PB", "1P", "1EB", "1E", "1ZB", "1Z", "1YB", "1Y", "1RB", "1R",
        "1QB", "1Q",
    ):
        for flag in ("-n", "-c"):
            s.case("tail", [flag, value, lines])
        s.case("tail", ["--lines=" + value, lines])
        s.case("tail", ["--bytes=" + value, lines])

    # Lowercase k/m are supported, while lowercase giga-and-larger multipliers
    # are rejected. The iB spelling also accepts lowercase k/m prefixes.
    for value in ("1k", "1m", "1mB", "1kiB", "1miB"):
        for flag in ("-n", "-c"):
            s.case("tail", [flag, value, lines])
    for value in ("1g", "1z", "1q", "1gB", "1zB", "1qB", "1gib"):
        for flag in ("-n", "-c"):
            s.case("tail", [flag, value, lines])

    for args in (
        ["--help"],
        ["--hel"],
        ["-n", "1", lines, binary],
        ["-n", "0", lines, binary],
        ["-2", lines, binary],
        ["-2c", lines, binary],
        ["+2", lines, binary],
        ["+2", "--", lines, binary],
        ["-v", "-n", "0", lines],
        ["-n", "+99", lines, binary],
        ["+2", "-n", "1", lines],
        ["-2", "-q", lines],
        ["-2c", "-n", "1", lines],
        ["-2", lines, "-q"],
        ["-v", "-n", "1", empty],
        ["-q", "-n", "1", lines, binary],
        ["-v", "-n", "1", lines, binary],
        ["-vq", "-n", "1", lines, binary],
        ["-qv", "-n", "1", lines, binary],
        ["--verbose", "--quiet", "-n", "1", lines, binary],
        ["--quiet", "--verbose", "-n", "1", lines, binary],
        ["-qn2", lines],
        ["-qc3", binary],
        ["-qvn", "2", lines],
        ["-s0.001", lines],
        ["-s"],
        ["-n", "1", lines, missing],
        [unusual_name, unusual_name],
        [invalid_byte_name],
        [missing_unusual],
        [missing_quote_control],
        ["-n", "1", missing, lines],
        ["-n", "9223372036854775808", lines],
        ["-c", "9223372036854775808", binary],
        ["-n", "18446744073709551616", lines],
        ["-c", "1Q", binary],
        ["-n", "invalid"],
        ["-c", "invalid"],
        ["--lines=invalid"],
        ["--bytes=invalid"],
        ["--bogus"],
        ["-x"],
        ["-n"],
        ["-c"],
        ["--lines"],
        ["--bytes"],
        ["--max-unchanged-stats=bad"],
        ["--max-unchanged-stats=0"],
        ["--max-unchanged-stats", "bad"],
        ["--pid=bad"],
        ["--pid"],
        ["--sleep-interval=bad"],
        ["--sleep-interval=NaN"],
        ["--sleep-interval=-1"],
        ["--sleep-interval=1 "],
        ["--sleep-interval"],
        ["--follow=bad"],
        ["--follow="],
        ["--retry=1"],
        ["--disable-inotify"],
        ["--v"],
        ["--ver"],
        ["--s"],
        ["--pid=0"],
        ["--sleep-interval=.1", lines],
        ["--sleep-interval=1.", lines],
        ["--sleep-interval=0.00000001", lines],
        ["--sleep-interval=1e-3", lines],
        ["--sleep-interval=inf", lines],
        ["-f", missing],
        ["--follow=name", missing],
        ["--", "-n"],
    ):
        s.case("tail", args)
    s.case("tail", ["-n", "2"], stdin=b"one\ntwo\nthree\n")
    s.case("tail", [lines, "-", "-"], stdin=b"stdin is consumed once\n")
    s.case("tail", ["-c", "4"], stdin=b"a\0b\n\xff")
    s.permission_denied_case("tail", ["-n", "1", unreadable, lines], unreadable)
    _follow(s, "descriptor")
    _follow(s, "descriptor-truncate", truncation=True)
    _follow(s, "descriptor-multi", multi_file=True)
    _follow(s, "name-multi", multi_file=True)
    _follow(s, "quiet-multi", multi_file=True, quiet=True)
    _follow(s, "verbose-single", verbose=True)
    _follow(s, "name-multi-retry", initial_missing=True, multi_file=True)
    _follow(s, "legacy-follow")
    _follow(s, "descriptor-long")
    _follow(s, "name")
    _follow(s, "name-abbrev")
    _follow(s, "name-rotation", rotation=True)
    _follow(s, "F-rotation", rotation=True)
    _follow(s, "name-retry", initial_missing=True)
    _follow(s, "descriptor-pid", pid_bound=True)
    _follow_write_failure(s)
    _debug_report(s)

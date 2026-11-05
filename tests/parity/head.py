"""Parity cases for head."""

import os
import select
import subprocess

from support import Suite


def run(s: Suite) -> None:
    s.product_version("head")
    empty = s.fixture("head-empty", b"")
    lines = s.fixture("head-lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("head-no-final-newline", b"a\nb\nc")
    binary = s.fixture("head-binary", b"a\0b\n\xffz")
    large = s.fixture("head-large", b"row\n" * 25000)
    missing = s.path("head-missing")
    unusual_name = s.fixture("head-line\nbreak", b"name with newline\n")
    missing_unusual = s.path("head-missing-line\nbreak")
    missing_quote_control = s.path("head-missing-'line\nbreak")
    s.write_failure("head", [lines])
    s.write_failure("head", ["--help"])
    s.write_failure("head", ["--version"])
    s.symlink_dispatch("head", ["-n", "1", lines])

    options = (
        [],
        ["-n", "2"],
        ["-n2"],
        ["-2"],
        ["-2c"],
        ["-2b"],
        ["-2l"],
        ["-2k"],
        ["-2m"],
        ["-2kb"],
        ["-2bc"],
        ["-2cb"],
        ["-2cl"],
        ["-2lc"],
        ["-n", "-1"],
        ["-n", "-0"],
        ["-n", "+2"],
        ["-c", "5"],
        ["-c5"],
        ["-c", "-2"],
        ["-c", "+3"],
        ["--bytes=5"],
        ["--lines=2"],
        ["-z", "-n", "1"],
        ["-q", "-n", "1"],
        ["-v", "-n", "1"],
        ["-n", "100000"],
    )
    fixtures = (empty, lines, no_final_newline, binary, large)
    for opts in options:
        for fixture in fixtures:
            s.case("head", [*opts, fixture])

    # Exercise signed counts, GNU size suffixes, and both attached and long
    # forms across empty, unterminated, binary, and streamed file contents.
    for value in ("0", "1", "2", "-1", "-2", "+1", "+2", "-0", "1K", "1b"):
        for flag in ("-n", "-c"):
            for fixture in fixtures:
                s.case("head", [flag, value, fixture])
        for flag in ("--lines=", "--bytes="):
            for fixture in (empty, lines, binary):
                s.case("head", [flag + value, fixture])

    for option in ("--bytes", "--lines"):
        for length in range(3, len(option) + 1):
            s.case("head", [option[:length] + "=2", lines])
    for option in ("--quiet", "--silent", "--verbose", "--zero-terminated"):
        for length in range(3, len(option) + 1):
            s.case("head", [option[:length], "-n", "1", lines])

    for value in (
        "1b", "1kB", "1K", "1KiB", "1MB", "1M", "1GB", "1G", "1TB", "1T",
        "1PB", "1P", "1EB", "1E", "1ZB", "1Z", "1YB", "1Y", "1RB", "1R",
        "1QB", "1Q",
    ):
        for flag in ("-n", "-c"):
            s.case("head", [flag, value, lines])
        s.case("head", ["--lines=" + value, lines])
        s.case("head", ["--bytes=" + value, lines])

    # GNU accepts lowercase k/m multipliers, but only uppercase multipliers
    # from giga upward. Binary iB suffixes follow the same rule.
    for value in ("1kB", "1mB", "1kiB", "1miB", "1K", "1M", "1G", "1GiB", "1Q"):
        for flag in ("-n", "-c"):
            s.case("head", [flag, value, lines])
    for value in ("1g", "1z", "1q", "1gB", "1zB", "1qB", "1gib"):
        for flag in ("-n", "-c"):
            s.case("head", [flag, value, lines])

    for mask in range(1 << 3):
        flags = "".join(flag for bit, flag in enumerate("qvz") if mask & (1 << bit))
        args = (["-" + flags] if flags else []) + ["-n", "1", lines, binary]
        s.case("head", args)

    for args in (
        ["--help"],
        ["--hel"],
        ["-n", "1", lines, binary],
        ["-2f", lines],
        ["-2K", lines],
        ["-2q", lines, binary],
        ["-2v", lines, binary],
        ["-1qv", lines, binary],
        ["-1vq", lines, binary],
        ["-2z", binary],
        ["-q", "-n", "1", lines, binary],
        ["-v", "-n", "1", lines, binary],
        ["-vq", "-n", "1", lines, binary],
        ["-qv", "-n", "1", lines, binary],
        ["--verbose", "--quiet", "-n", "1", lines, binary],
        ["--quiet", "--verbose", "-n", "1", lines, binary],
        ["-n", "1", lines, missing],
        [unusual_name, unusual_name],
        [missing_unusual],
        [missing_quote_control],
        ["-n", "1", missing, lines],
        ["-n", "2", "-c", "3", lines],
        ["-c", "3", "-n", "2", lines],
        ["-qn2", lines],
        ["-qc3", lines],
        ["-qvzn", "2", lines],
        ["-vn", "2", lines],
        ["-n", "-2"],
        ["-c", "-2"],
        ["--", "-n"],
        ["-x"],
        ["--bogus"],
        ["-n"],
        ["--lines"],
        ["-c"],
        ["--bytes"],
        ["-n", "invalid"],
        ["-c", "invalid"],
        ["-n", "9223372036854775808"],
        ["-c", "9223372036854775808"],
        ["-n", "not-a-number"],
        ["-c", "not-a-number"],
        ["--bytes=not-a-number"],
        ["--lines=not-a-number"],
        ["--quiet=1"],
        ["--bytes="],
        ["--lines="],
        ["--verbose="],
        ["--help="],
        ["--version="],
        ["--v"],
        ["--ver"],
    ):
        s.case("head", args)
    s.case("head", ["-n", "2"], stdin=b"one\ntwo\nthree\n")
    s.case("head", [lines, "-", "-"], stdin=b"stdin is consumed once\n")
    s.case("head", ["-c", "4"], stdin=b"a\0b\n\xff")
    s.case("head", ["-n", "-1"], stdin=b"one\ntwo\nthree\n")
    s.case("head", ["-c", "-2"], stdin=b"a\0b\n\xff")
    s.case("head", ["-n", "0"], stdin=b"")

    # Positive-count modes emit as soon as enough bytes arrive, before stdin
    # reaches EOF. Check newline, byte, and NUL-delimited reads against GNU.
    for opts, payload in (
        (["-n", "1"], b"abc\n"),
        (["-c", "4"], b"abc\n"),
        (["-z", "-n", "1"], b"abc\0"),
    ):
        outcomes = []
        for command in (
            [str(s.zovutils), "head", *opts],
            [str(s.gnu_dir / "head"), *opts],
        ):
            proc = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=s.env,
            )
            assert proc.stdin is not None and proc.stdout is not None
            proc.stdin.write(payload)
            proc.stdin.flush()
            ready, _, _ = select.select([proc.stdout], [], [], 2)
            early = os.read(proc.stdout.fileno(), 64) if ready else b""
            proc.stdin.close()
            remainder = proc.stdout.read()
            stderr = proc.stderr.read() if proc.stderr is not None else b""
            status = proc.wait(timeout=5)
            outcomes.append((bool(ready), early, remainder, status, stderr))
        s.count += 1
        if outcomes[0] != outcomes[1]:
            raise AssertionError(f"head streaming behavior differs: {opts!r}: {outcomes!r}")

    # Reading and writing the same regular file must stop at its original EOF.
    same_path = s.path("head-same-output")
    same_results = []
    for command in (
        [str(s.zovutils), "head", same_path],
        [str(s.gnu_dir / "head"), same_path],
    ):
        with open(same_path, "wb") as fixture:
            fixture.write(b"abcdefghij\n")
        with open(same_path, "r+b") as output:
            output.seek(3)
            proc = subprocess.run(
                command,
                stdout=output,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=5,
                check=False,
            )
        same_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
    s.count += 1
    if same_results[0] != same_results[1]:
        raise AssertionError(f"head same-file output handling differs: {same_results!r}")

    duplicate_results = []
    for command in (
        [str(s.zovutils), "head", "-n", "10", same_path, same_path],
        [str(s.gnu_dir / "head"), "-n", "10", same_path, same_path],
    ):
        with open(same_path, "wb") as fixture:
            fixture.write(b"a\nb\nc\n")
        with open(same_path, "r+b") as output:
            output.seek(2)
            proc = subprocess.run(
                command,
                stdout=output,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=5,
                check=False,
            )
        duplicate_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
    s.count += 1
    if duplicate_results[0] != duplicate_results[1]:
        raise AssertionError(f"head duplicate same-file output differs: {duplicate_results!r}")

"""Parity cases for head."""

import os
import resource
import select
import signal
import subprocess

from support import Suite


def _limit_output_file() -> None:
    resource.setrlimit(resource.RLIMIT_FSIZE, (65536, 65536))
    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)


def _nonblocking_input_cases(s: Suite) -> None:
    if not hasattr(os, "set_blocking"):
        return

    results = []
    for command in (
        [str(s.zovutils), "head"],
        [str(s.gnu_dir / "head")],
    ):
        modes = []
        for options in ((), ("-n", "1"), ("-n", "-1"), ("-c", "3"), ("-c", "-1")):
            read_fd, write_fd = os.pipe()
            os.set_blocking(read_fd, False)
            os.write(write_fd, b"a\nb\n")
            try:
                proc = subprocess.Popen(
                    [*command, *options],
                    stdin=read_fd,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    env=s.env,
                )
            finally:
                os.close(read_fd)
            try:
                stdout, stderr = proc.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.communicate()
                raise AssertionError("head did not stop after nonblocking input")
            finally:
                os.close(write_fd)
            modes.append((proc.returncode, stdout, stderr))
        results.append(modes)

    s.count += 5
    if results[0] != results[1]:
        raise AssertionError(f"head nonblocking-input behavior differs: {results!r}")


def run(s: Suite) -> None:
    _nonblocking_input_cases(s)
    s.product_version("head")
    if os.path.exists("/dev/zero"):
        s.case("head", ["-c", "4096", "/dev/zero"])

    empty = s.fixture("head-empty", b"")
    lines = s.fixture("head-lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("head-no-final-newline", b"a\nb\nc")
    binary = s.fixture("head-binary", b"a\0b\n\xffz")
    large = s.fixture("head-large", b"row\n" * 25000)
    # Exercise negative-count elision immediately around the streaming read
    # boundary, for both byte and line modes.
    for size in (65535, 65536, 65537):
        payload = bytes(index % 251 for index in range(size))
        fixture = s.fixture(f"head-elide-bytes-{size}", payload)
        for count in (0, 1, 2, 8192, 65535, 65536, 65537):
            s.case("head", ["--bytes=-" + str(count), fixture])
    line_boundary = s.fixture("head-elide-line-boundary", b"x\n" * 32768 + b"tail")
    for count in (0, 1, 2, 32767, 32768, 32769):
        s.case("head", ["--lines=-" + str(count), line_boundary])
    missing = s.path("head-missing")
    unusual_name = s.fixture("head-line\nbreak", b"name with newline\n")
    invalid_byte_name = s.fixture("head-invalid-\udcff", b"name with invalid byte\n")
    missing_unusual = s.path("head-missing-line\nbreak")
    missing_quote_control = s.path("head-missing-'line\nbreak")
    unreadable = s.fixture("head-unreadable", b"unreadable\n")
    s.write_failure("head", [lines])
    s.write_failure("head", ["--help"])
    s.write_failure("head", ["--version"])
    for mode in ("lines", "bytes"):
        for count in ("-0", "-1"):
            s.write_failure("head", [f"--{mode}={count}", lines])
    for mode in ("lines", "bytes"):
        for count in ("-0", "-1"):
            s.write_failure("head", [f"--{mode}={count}", large])
    s.broken_pipe_case("head", ["-c", "100000", large])
    s.interrupted_read_case("head", ["-c", "2"])
    s.read_error_after_partial_case("head", [])
    s.pty_read_error_case("head", [])
    s.symlink_dispatch("head", ["-n", "1", lines])
    s.case("head", ["-n", "1", lines], extra_env={"POSIXLY_CORRECT": "1"})
    s.case("head", [lines, "-n", "1"], extra_env={"POSIXLY_CORRECT": ""})

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
    for value in ("0", "1", "2", "-1", "-2", "+1", "+2", "-0", "1K", "1b", "k", "K", "m", "M", "b", "kB", "KiB", "-k", "+k", "g"):
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
        ["---"],
        ["---", lines],
        [lines, "---", binary],
        ["-n", "1", lines, binary],
        ["-2f", lines],
        ["-2K", lines],
        ["-2q", lines, binary],
        ["-2v", lines, binary],
        ["-z", "-2", lines],
        ["-q", "-2", lines],
        [lines, "-2"],
        ["-n", "2", "-2", lines],
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
        [invalid_byte_name],
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
        ["-z5"],
        ["-q5"],
        ["-qxx"],
        ["--bogus"],
        ["-n"],
        ["--lines"],
        ["-c"],
        ["--bytes"],
        ["-n", "invalid"],
        ["-c", "invalid"],
        ["-n", "-q"],
        ["-c", "-q"],
        ["-n", "-1foo"],
        ["-c", "--bad"],
        ["-n", "-"],
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
    s.permission_denied_case("head", ["-n", "1", unreadable, lines], unreadable)

    # Preserve the input descriptor position so a following reader sees the
    # bytes head left behind. This matches GNU's head-pos and head-c checks.
    positioned = s.fixture("head-positioned-stdin", b"one\ntwo\nthree\n")
    for opts in (
        ["-c", "0"],
        ["-c", "2"],
        ["-c", "-2"],
        ["-n", "0"],
        ["-n", "1"],
        ["-n", "-1"],
    ):
        for offset in (0, 4, 8):
            outcomes = []
            for command in (
                [str(s.zovutils), "head", *opts],
                [str(s.gnu_dir / "head"), *opts],
            ):
                with open(positioned, "rb") as source:
                    source.seek(offset)
                    proc = subprocess.run(
                        command,
                        stdin=source,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        env=s.env,
                        timeout=5,
                        check=False,
                    )
                    remaining = bytearray()
                    while True:
                        chunk = os.read(source.fileno(), 65536)
                        if not chunk:
                            break
                        remaining.extend(chunk)
                outcomes.append((proc.returncode, proc.stdout, proc.stderr, bytes(remaining)))
            s.count += 1
            if outcomes[0] != outcomes[1]:
                raise AssertionError(
                    f"head input-position behavior differs: "
                    f"options={opts!r} offset={offset}: {outcomes!r}"
                )

    large_positioned = s.fixture(
        "head-large-positioned-stdin",
        b"".join(f"{line:05d}\n".encode() for line in range(70000)),
    )
    large_position_results = []
    for command in (
        [str(s.zovutils), "head", "-n", "-50000"],
        [str(s.gnu_dir / "head"), "-n", "-50000"],
    ):
        with open(large_positioned, "rb") as source:
            proc = subprocess.run(
                command,
                stdin=source,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=10,
                check=False,
            )
            remaining = bytearray()
            while True:
                chunk = os.read(source.fileno(), 65536)
                if not chunk:
                    break
                remaining.extend(chunk)
        large_position_results.append(
            (proc.returncode, proc.stdout, proc.stderr, bytes(remaining))
        )
    s.count += 1
    if large_position_results[0] != large_position_results[1]:
        raise AssertionError(
            f"head large negative-line input position differs: {large_position_results!r}"
        )

    for option in ("---", "---p", "---pres", "---presume-input-pipe"):
        s.case("head", [option, "--bytes=-2", binary])

    presumed_pipe_results = []
    for command in (
        [str(s.zovutils), "head", "---presume-input-pipe", "-n", "-50000"],
        [str(s.gnu_dir / "head"), "---presume-input-pipe", "-n", "-50000"],
    ):
        with open(large_positioned, "rb") as source:
            proc = subprocess.run(
                command,
                stdin=source,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=10,
                check=False,
            )
            remaining = bytearray()
            while True:
                chunk = os.read(source.fileno(), 65536)
                if not chunk:
                    break
                remaining.extend(chunk)
        presumed_pipe_results.append(
            (proc.returncode, proc.stdout, proc.stderr, bytes(remaining))
        )
    s.count += 1
    if presumed_pipe_results[0] != presumed_pipe_results[1]:
        raise AssertionError(
            f"head presumed-pipe input position differs: {presumed_pipe_results!r}"
        )

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

    for opts in (["-c", "2"], ["-c", "-2"], ["-n", "-1"], ["-z", "-n", "-1"]):
        for input_offset in (0, 2, 6):
            for output_offset in (0, 2, 6):
                outcomes = []
                for command in (
                    [str(s.zovutils), "head", *opts, "-"],
                    [str(s.gnu_dir / "head"), *opts, "-"],
                ):
                    with open(same_path, "wb") as fixture:
                        fixture.write(b"a\nb\nc\n")
                    with open(same_path, "rb") as source, open(same_path, "r+b") as output:
                        source.seek(input_offset)
                        output.seek(output_offset)
                        proc = subprocess.run(
                            command,
                            stdin=source,
                            stdout=output,
                            stderr=subprocess.PIPE,
                            env=s.env,
                            timeout=5,
                            check=False,
                        )
                    outcomes.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
                s.count += 1
                if outcomes[0] != outcomes[1]:
                    raise AssertionError(
                        f"head same-file stdin/output offsets differ: "
                        f"options={opts!r} input={input_offset} output={output_offset}: {outcomes!r}"
                    )

    # Duplicated stdin and stdout descriptors share one cursor. Positive head
    # modes must rewind buffered read-ahead before writing; negative modes
    # write at the retained prefix boundary after spooling the input.
    for opts in ([], ["-c", "2"], ["-c", "-2"], ["-n", "1"], ["-n", "-1"], ["-z", "-n", "1"]):
        for offset in (0, 2, 6):
            outcomes = []
            for command in (
                [str(s.zovutils), "head", *opts, "-"],
                [str(s.gnu_dir / "head"), *opts, "-"],
            ):
                with open(same_path, "wb") as fixture:
                    fixture.write(b"a\nb\nc\n")
                with open(same_path, "r+b") as stream:
                    stream.seek(offset)
                    proc = subprocess.run(
                        command,
                        stdin=stream,
                        stdout=stream,
                        stderr=subprocess.PIPE,
                        env=s.env,
                        timeout=5,
                        check=False,
                        preexec_fn=_limit_output_file,
                    )
                outcomes.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
            s.count += 1
            if outcomes[0] != outcomes[1]:
                raise AssertionError(
                    f"head shared-descriptor behavior differs: "
                    f"options={opts!r} offset={offset}: {outcomes!r}"
                )

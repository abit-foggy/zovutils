"""Parity cases for cat."""

import os
import resource
import select
import signal
import subprocess

from support import Suite


def _limit_output_file() -> None:
    resource.setrlimit(resource.RLIMIT_FSIZE, (65536, 65536))
    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)


def run(s: Suite) -> None:
    empty = s.fixture("empty", b"")
    text = s.fixture("text", b"alpha\nbeta\n\ngamma\n")
    lines = s.fixture("lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("no-final-newline", b"no-final-newline")
    binary = s.fixture("binary", b"a\0b\n\xffz")
    tabs = s.fixture("tabs", b"a\tb\n\t\n")
    all_bytes = s.fixture("all-bytes", bytes(range(256)))
    boundary = s.fixture("boundary", b"x" * 65535 + b"\n\n\nend")
    large = s.fixture("large", (b"stream\0bytes\xff\n" * 131072) + b"tail")
    split_first = s.fixture("split-first", b"first")
    split_second = s.fixture("split-second", b"second\n\n")
    split_blank = s.fixture("split-blank", b"\nlast\n")
    unusual_name = s.fixture("cat-line\nbreak", b"name with newline\n")
    missing_unusual = s.path("cat-missing-line\nbreak")
    missing_quote_control = s.path("cat-missing-'line\nbreak")
    missing_control_quote = s.path("cat-missing-\x07'control")
    missing_shell_meta = [
        s.path("cat-missing " + token)
        for token in ("space", "=", "!", ":", "[", "|")
    ] + [s.path("cat-missing'" + token) for token in ("!", "$", '"')]
    directory = s.path("directory")
    s.root.joinpath("directory").mkdir()

    # Exercise every combination of cat's independent display/numbering flags
    # against text, binary, and chunk-boundary inputs.
    switches = "nbsETv"
    for mask in range(1 << len(switches)):
        selected = "".join(flag for bit, flag in enumerate(switches) if mask & (1 << bit))
        opts = ["-" + selected] if selected else []
        for fixture in (text, binary, boundary):
            s.case("cat", [*opts, fixture])

    # GNU accepts every unambiguous abbreviation of a long option and reports
    # the full candidate list for ambiguous prefixes.
    for option in (
        "--number",
        "--number-nonblank",
        "--squeeze-blank",
        "--show-all",
        "--show-ends",
        "--show-tabs",
        "--show-nonprinting",
    ):
        for length in range(3, len(option) + 1):
            s.case("cat", [option[:length], text])

    for args in (
        ["--help"],
        ["--h"],
        ["--he"],
        ["--hel"],
        [],
        [text],
        ["-n", text],
        ["-b", text],
        ["-n", "-b", text],
        ["-s", text],
        ["-n", "-s", text],
        ["-A", binary],
        ["-v", binary],
        ["-E", no_final_newline],
        ["-T", tabs],
        ["-t", tabs],
        ["-e", binary],
        ["-u", text],
        ["--number", text],
        ["--number-nonblank", text],
        ["--squeeze-blank", text],
        ["--show-ends", text],
        ["--show-tabs", tabs],
        ["--show-nonprinting", all_bytes],
        ["--show-all", all_bytes],
        ["--show-a", all_bytes],
        ["--show-e", all_bytes],
        ["--show-t", tabs],
        ["--show-n", all_bytes],
        ["--squeez", text],
        ["--number-n", text],
        ["--n"],
        ["--s"],
        ["-v", all_bytes],
        ["-A", all_bytes],
        ["-e", all_bytes],
        ["-t", all_bytes],
        ["-A", "-n", "-s", all_bytes],
        ["-n", "-b", "-n", text],
        ["-n", "-b", "-b", "-n", text],
        ["-b", "-s", text],
        ["-n", boundary],
        ["-s", boundary],
        [large],
        [unusual_name],
        ["-A", "-e", all_bytes],
        ["-A", "-t", all_bytes],
        ["-e", "-t", all_bytes],
        ["-A", "-v", all_bytes],
        ["-u", "-A", all_bytes],
        ["-u", "-b", "-n", text],
        ["-n", "-b", "-s", "-E", "-T", "-v", boundary],
        [text, "-n"],
        [text, lines],
        ["-s", split_second, split_blank],
        ["-n", split_first, split_second],
        [split_first, "-", split_second],
        ["--", "-n"],
        ["--", "--"],
        [text, "--", "--", text],
        [directory],
        [s.path("missing")],
        [missing_unusual],
        [missing_quote_control],
        [missing_control_quote],
        *[["--", path] for path in missing_shell_meta],
        [text, s.path("missing"), lines],
        [empty, text],
        ["-x"],
        ["--bogus"],
        ["--show-all=bad"],
        ["--number="],
        ["--number-nonblank="],
        ["--squeeze-blank="],
        ["--show-all="],
        ["--show-ends="],
        ["--show-tabs="],
        ["--show-nonprinting="],
        ["--help=x"],
        ["--hel=x"],
        ["--version=x"],
        ["--ver=x"],
        ["--num=x"],
        ["--nu=x"],
        ["--n=x"],
        ["--s=x"],
        ["--show=x"],
        ["--show-a=x"],
        ["--squeeze=x"],
        ["--squeez=x"],
        ["--number-n=x"],
        ["--show-e=x"],
        ["--show-t=x"],
        ["--show-non=x"],
        ["--bogus=x"],
    ):
        s.case("cat", args, stdin=b"from-stdin\n")
    s.case("cat", [], stdin=b"stdin\0bytes\n")
    s.case("cat", ["-", "-"], stdin=b"stdin is consumed once\n")

    # cat must forward each available input chunk before stdin reaches EOF.
    # Captured stdout is fully buffered by stdio unless cat flushes it.
    for opts in ([], ["-n"], ["-v"], ["-u"]):
        outcomes = []
        for command in (
            [str(s.zovutils), "cat", *opts],
            [str(s.gnu_dir / "cat"), *opts],
        ):
            proc = subprocess.Popen(
                command,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=s.env,
            )
            assert proc.stdin is not None and proc.stdout is not None
            proc.stdin.write(b"stream")
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
            raise AssertionError(f"cat streaming behavior differs: {opts!r}: {outcomes!r}")

    # GNU refuses to read an input inode that is also stdout. Bound the child
    # file size so a regression cannot grow it without limit.
    same_path = s.path("cat-same-output")
    same_results = []
    for command in (
        [str(s.zovutils), "cat", same_path],
        [str(s.gnu_dir / "cat"), same_path],
    ):
        with open(same_path, "wb") as fixture:
            fixture.write(b"abcdef")
        with open(same_path, "r+b") as output:
            output.seek(2)
            proc = subprocess.run(
                command,
                stdout=output,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=5,
                check=False,
                preexec_fn=_limit_output_file,
            )
        same_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
    s.count += 1
    if same_results[0] != same_results[1]:
        raise AssertionError(f"cat same-file output handling differs: {same_results!r}")

    # GNU's cat behavior through an installed command-name symlink.
    alias = s.root / "cat"
    alias.symlink_to(s.zovutils)
    zov = s._run([str(alias), "-n", text], b"")
    gnu = s._run([str(s.gnu_dir / "cat"), "-n", text], b"")
    s.count += 1
    if zov != gnu:
        raise AssertionError("cat argv[0] symlink dispatch differs from GNU")

    s.product_version("cat")
    s.write_failure("cat", [text])
    s.write_failure("cat", ["--help"])
    s.write_failure("cat", ["--version"])

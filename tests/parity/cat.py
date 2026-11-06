"""Parity cases for cat."""

import os
import random
import re
import resource
import select
import signal
import subprocess
import sys
from pathlib import Path

from support import Suite


def _limit_output_file() -> None:
    resource.setrlimit(resource.RLIMIT_FSIZE, (65536, 65536))
    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)


def _nonblocking_input_cases(s: Suite) -> None:
    if sys.platform != "linux":
        return

    results = []
    for command in (
        [str(s.zovutils), "cat"],
        [str(s.gnu_dir / "cat")],
    ):
        modes = []
        for options in ((), ("-n",), ("-v",)):
            read_fd, write_fd = os.pipe()
            os.set_blocking(read_fd, False)
            os.write(write_fd, b"available\nbytes\xff")
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
                raise AssertionError("cat did not stop at EAGAIN")
            finally:
                os.close(write_fd)
            modes.append((proc.returncode, stdout, stderr))
        results.append(modes)

    s.count += 3
    if results[0] != results[1]:
        raise AssertionError(f"cat nonblocking-input behavior differs: {results!r}")


def _write_error_cases(s: Suite) -> None:
    if not Path("/dev/full").exists():
        return

    results = []
    for command in (
        [str(s.zovutils), "cat"],
        [str(s.gnu_dir / "cat")],
    ):
        modes = []
        for options in ((), ("-n",), ("-v",)):
            with open("/dev/full", "wb") as output:
                proc = subprocess.run(
                    [*command, *options],
                    input=b"write failure\nwith multiple lines\n",
                    stdout=output,
                    stderr=subprocess.PIPE,
                    env=s.env,
                    timeout=5,
                    check=False,
                )
            modes.append((proc.returncode, proc.stderr))
        results.append(modes)

    s.count += 3
    if results[0] != results[1]:
        raise AssertionError(f"cat output-error behavior differs: {results!r}")


def run(s: Suite) -> None:
    _nonblocking_input_cases(s)
    _write_error_cases(s)
    empty = s.fixture("empty", b"")
    text = s.fixture("text", b"alpha\nbeta\n\ngamma\n")
    lines = s.fixture("lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("no-final-newline", b"no-final-newline")
    binary = s.fixture("binary", b"a\0b\n\xffz")
    tabs = s.fixture("tabs", b"a\tb\n\t\n")
    crlf = s.fixture("crlf", b"a\r\n\r\nend\rX\n")
    all_bytes = s.fixture("all-bytes", bytes(range(256)))
    boundary = s.fixture("boundary", b"x" * 65535 + b"\n\n\nend")
    crlf_boundary = s.fixture("crlf-boundary", b"x" * 65535 + b"\r\n")
    large = s.fixture("large", (b"stream\0bytes\xff\n" * 131072) + b"tail")
    split_first = s.fixture("split-first", b"first")
    split_second = s.fixture("split-second", b"second\n\n")
    split_cr = s.fixture("split-cr", b"split\r")
    split_lf = s.fixture("split-lf", b"\nend")
    split_blank = s.fixture("split-blank", b"\nlast\n")
    unusual_name = s.fixture("cat-line\nbreak", b"name with newline\n")
    invalid_byte_name = s.fixture("cat-invalid-\udcff", b"name with invalid byte\n")
    missing_unusual = s.path("cat-missing-line\nbreak")
    missing_quote_control = s.path("cat-missing-'line\nbreak")
    unreadable = s.fixture("cat-unreadable", b"unreadable\n")
    missing_control_quote = s.path("cat-missing-\x07'control")
    missing_shell_meta = [
        s.path("cat-missing " + token)
        for token in ("space", "=", "!", ":", "[", "|")
    ] + [s.path("cat-missing'" + token) for token in ("!", "$", '"')]
    directory = s.path("directory")
    s.root.joinpath("directory").mkdir()

    # Exercise every combination of cat's independent display/numbering flags
    # against text, binary, and chunk-boundary inputs.
    switches = "nbsETvAetu"
    for mask in range(1 << len(switches)):
        selected = "".join(flag for bit, flag in enumerate(switches) if mask & (1 << bit))
        opts = ["-" + selected] if selected else []
        for fixture in (text, binary, boundary, crlf):
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
        ["-E", crlf],
        ["-E", crlf_boundary],
        ["-vE", crlf_boundary],
        ["-sE", crlf_boundary],
        ["-n", crlf_boundary],
        ["-b", crlf_boundary],
        ["-nsE", crlf_boundary],
        ["-vE", crlf],
        ["-sE", crlf],
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
        [invalid_byte_name],
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
        ["-n", split_first, "-", split_second],
        ["-E", split_cr, split_lf],
        ["-E", split_cr],
        ["-b", split_blank, split_second],
        ["-n", "-s", split_second, split_blank],
        ["-b", "-s", split_blank, split_second],
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
    # GNU getopt stops at the first operand when POSIXLY_CORRECT is present.
    s.case("cat", [text, "-n"], extra_env={"POSIXLY_CORRECT": "1"})
    s.case("cat", [text, "--show-ends", lines], extra_env={"POSIXLY_CORRECT": ""})
    s.case("cat", [], stdin=b"stdin\0bytes\n")
    s.case("cat", ["-", "-"], stdin=b"stdin is consumed once\n")
    s.permission_denied_case("cat", [unreadable, text], unreadable)

    proc_cpuinfo = Path("/proc/cpuinfo")
    if sys.platform == "linux" and proc_cpuinfo.is_file():
        normalized = []
        for command in (
            [str(s.zovutils), "cat"],
            [str(s.gnu_dir / "cat")],
        ):
            plain = s._run([*command, str(proc_cpuinfo)], b"")
            visible = s._run([*command, "-E", str(proc_cpuinfo)], b"")
            if plain[0] != 0 or visible[0] != 0 or plain[2] or visible[2]:
                raise AssertionError(
                    f"cat /proc/cpuinfo failed: plain={plain!r} visible={visible!r}"
                )
            normalize = lambda data: re.sub(rb"[0-9]+", b"D", data).replace(b"$", b"")
            normalized.append((normalize(plain[1]), normalize(visible[1])))
        s.count += 1
        if normalized[0] != normalized[1] or normalized[0][0] != normalized[0][1]:
            raise AssertionError(f"cat -E /proc/cpuinfo differs from cat: {normalized!r}")

    # Force the first stdin read to return EINTR. GNU retries this transient
    # error, so cat must resume reading and preserve the input bytes.
    if sys.platform == "linux":
        interposer = s.root / "interrupt-read.so"
        interposer_source = s.root / "interrupt-read.c"
        interposer_source.write_text(
            """
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
static ssize_t (*next_read)(int, void *, size_t);
static int injected;
ssize_t read(int fd, void *buffer, size_t count) {
    if (!next_read) next_read = dlsym(RTLD_NEXT, "read");
    if (fd == STDIN_FILENO && !injected++) {
        errno = EINTR;
        return -1;
    }
    return next_read(fd, buffer, count);
}
"""
        )
        subprocess.run(
            ["cc", "-shared", "-fPIC", str(interposer_source), "-ldl", "-o", str(interposer)],
            check=True,
            timeout=15,
        )
        s.case(
            "cat",
            [],
            stdin=b"resumed after interrupted read\n",
            extra_env={"LD_PRELOAD": str(interposer)},
        )

        error_interposer = s.root / "error-read.so"
        error_source = s.root / "error-read.c"
        error_source.write_text(
            """
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
static ssize_t (*next_read)(int, void *, size_t);
static int injected;
ssize_t read(int fd, void *buffer, size_t count) {
    if (!next_read) next_read = dlsym(RTLD_NEXT, "read");
    if (fd == STDIN_FILENO && !injected++) {
        errno = EIO;
        return -1;
    }
    return next_read(fd, buffer, count);
}
"""
        )
        subprocess.run(
            ["cc", "-shared", "-fPIC", str(error_source), "-ldl", "-o", str(error_interposer)],
            check=True,
            timeout=15,
        )
        s.case(
            "cat",
            ["-v"],
            stdin=b"read error is reported\n",
            extra_env={"LD_PRELOAD": str(error_interposer)},
        )
        s.read_error_after_partial_case("cat", ["-v"])
    s.pty_read_error_case("cat", [])

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

    same_alias = s.path("cat-same-output-alias")
    os.symlink(same_path, same_alias)
    for source, payload, offset in (
        (same_path, b"abcdef", 0),
        (same_alias, b"abcdef", 0),
        (same_path, b"", 0),
    ):
        safe_same_results = []
        for command in (
            [str(s.zovutils), "cat", source],
            [str(s.gnu_dir / "cat"), source],
        ):
            with open(same_path, "wb") as fixture:
                fixture.write(payload)
            with open(same_path, "r+b") as output:
                output.seek(offset)
                proc = subprocess.run(
                    command,
                    stdout=output,
                    stderr=subprocess.PIPE,
                    env=s.env,
                    timeout=5,
                    check=False,
                )
            safe_same_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
        s.count += 1
        if safe_same_results[0] != safe_same_results[1]:
            raise AssertionError(f"cat safe same-file no-op behavior differs: {safe_same_results!r}")

    same_hardlink = s.path("cat-same-output-hardlink")
    os.link(same_path, same_hardlink)
    hardlink_results = []
    for command in (
        [str(s.zovutils), "cat", same_hardlink],
        [str(s.gnu_dir / "cat"), same_hardlink],
    ):
        with open(same_path, "wb") as fixture:
            fixture.write(b"hardlink-self\n")
        with open(same_path, "r+b") as output:
            output.seek(0)
            proc = subprocess.run(
                command,
                stdout=output,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=5,
            )
        hardlink_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
    s.count += 1
    if hardlink_results[0] != hardlink_results[1]:
        raise AssertionError(f"cat hard-link same-file handling differs: {hardlink_results!r}")

    append_results = []
    for command in (
        [str(s.zovutils), "cat", same_path],
        [str(s.gnu_dir / "cat"), same_path],
    ):
        with open(same_path, "wb") as fixture:
            fixture.write(b"append-self\n")
        with open(same_path, "ab") as output:
            proc = subprocess.run(
                command,
                stdout=output,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=5,
                check=False,
            )
        append_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
    s.count += 1
    if append_results[0] != append_results[1]:
        raise AssertionError(f"cat append-mode same-file handling differs: {append_results!r}")

    stdin_append_results = []
    for command in (
        [str(s.zovutils), "cat"],
        [str(s.gnu_dir / "cat")],
    ):
        with open(same_path, "wb") as fixture:
            fixture.write(b"stdin-append-self\n")
        with open(same_path, "rb") as source, open(same_path, "ab") as output:
            proc = subprocess.run(
                command,
                stdin=source,
                stdout=output,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=5,
                check=False,
                preexec_fn=_limit_output_file,
            )
        stdin_append_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
    s.count += 1
    if stdin_append_results[0] != stdin_append_results[1]:
        raise AssertionError(f"cat stdin append-mode same-file handling differs: {stdin_append_results!r}")

    append_eof_results = []
    for command in (
        [str(s.zovutils), "cat", "-"],
        [str(s.gnu_dir / "cat"), "-"],
    ):
        with open(same_path, "wb") as fixture:
            fixture.write(b"append-at-eof\n")
        with open(same_path, "rb") as source, open(same_path, "ab") as output:
            source.seek(0, os.SEEK_END)
            proc = subprocess.run(
                command,
                stdin=source,
                stdout=output,
                stderr=subprocess.PIPE,
                env=s.env,
                timeout=5,
                check=False,
            )
        append_eof_results.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
    s.count += 1
    if append_eof_results[0] != append_eof_results[1]:
        raise AssertionError(f"cat append-mode EOF handling differs: {append_eof_results!r}")

    for input_offset in (0, 2, 6):
        for output_offset in (0, 2, 6):
            outcomes = []
            for command in (
                [str(s.zovutils), "cat", "-"],
                [str(s.gnu_dir / "cat"), "-"],
            ):
                with open(same_path, "wb") as fixture:
                    fixture.write(b"abcdef")
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
                        preexec_fn=_limit_output_file,
                    )
                outcomes.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
            s.count += 1
            if outcomes[0] != outcomes[1]:
                raise AssertionError(
                    f"cat same-file stdin/output offsets differ: "
                    f"input={input_offset} output={output_offset} outcomes={outcomes!r}"
                )

    # Transforming modes must read same-inode input at equal offsets. Some
    # modes expand output and GNU continues until the bounded destination
    # fails; squeeze mode can instead shorten the stream.
    transform_payload = b"a\n\n\t\xff\n"
    for opts in (["-n"], ["-b"], ["-v"], ["-E"], ["-T"], ["-A"], ["-e"], ["-t"], ["-s"], ["-u"]):
        for operand_mode in ("path", "stdin"):
            outcomes = []
            for command in (
                [str(s.zovutils), "cat", *opts],
                [str(s.gnu_dir / "cat"), *opts],
            ):
                with open(same_path, "wb") as fixture:
                    fixture.write(transform_payload)
                if operand_mode == "path":
                    args = [*command, same_path]
                    source_context = None
                else:
                    args = [*command, "-"]
                    source_context = open(same_path, "rb")
                try:
                    with open(same_path, "r+b") as output:
                        output.seek(0)
                        proc = subprocess.run(
                            args,
                            stdin=source_context,
                            stdout=output,
                            stderr=subprocess.PIPE,
                            env=s.env,
                            timeout=5,
                            check=False,
                            preexec_fn=_limit_output_file,
                        )
                    outcomes.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
                finally:
                    if source_context is not None:
                        source_context.close()
            s.count += 1
            if outcomes[0] != outcomes[1]:
                raise AssertionError(
                    f"cat transformed same-file handling differs: "
                    f"options={opts!r} operand={operand_mode}: {outcomes!r}"
                )

    # stdin and stdout can be dup()s of the same open file description. Reads
    # and writes then share one cursor, so even plain cat appends each chunk.
    for opts in ([], ["-n"]):
        for offset in (0, 2, 6):
            outcomes = []
            for command in (
                [str(s.zovutils), "cat", *opts, "-"],
                [str(s.gnu_dir / "cat"), *opts, "-"],
            ):
                with open(same_path, "wb") as fixture:
                    fixture.write(transform_payload)
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
                    f"cat shared-descriptor behavior differs: "
                    f"options={opts!r} offset={offset}: {outcomes!r}"
                )

    rng = random.Random(0x511)
    switch_flags = "nbsETvAetu"
    shared_payload = b"a\n\n\t\xff\x01\0b\n\nlast"
    for _ in range(100):
        selected = rng.sample(switch_flags, rng.randrange(len(switch_flags) + 1))
        opts = ["-" + "".join(selected)] if selected else []
        offset = rng.choice((0, 2, 6))
        outcomes = []
        for command in (
            [str(s.zovutils), "cat", *opts, "-"],
            [str(s.gnu_dir / "cat"), *opts, "-"],
        ):
            with open(same_path, "wb") as fixture:
                fixture.write(shared_payload)
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
                f"cat randomized shared-descriptor behavior differs: "
                f"options={opts!r} offset={offset}: {outcomes!r}"
            )

    # Random byte streams stress visible-byte conversion, blank-line state,
    # and state carried across adjacent files and stdin.
    byte_rng = random.Random(0xCA7)
    for index in range(64):
        payload = bytes(byte_rng.randrange(256) for _ in range(byte_rng.randrange(257)))
        selected = byte_rng.sample(switch_flags, byte_rng.randrange(len(switch_flags) + 1))
        opts = ["-" + "".join(selected)] if selected else []
        s.case("cat", [*opts, "-"], stdin=payload)

        split = byte_rng.randrange(len(payload) + 1)
        first = s.fixture(f"cat-random-{index}-first", payload[:split])
        second = s.fixture(f"cat-random-{index}-second", payload[split:])
        s.case("cat", [*opts, first, second])

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
    s.broken_pipe_case("cat", [large])

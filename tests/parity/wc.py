"""Parity cases for wc."""

import locale
import random
import subprocess

from support import Suite


def run(s: Suite) -> None:
    s.product_version("wc")
    empty = s.fixture("wc-empty", b"")
    text = s.fixture("wc-text", b"alpha\nbeta\n\ngamma\n")
    no_final_newline = s.fixture("wc-no-final-newline", b"no-final-newline")
    binary = s.fixture("wc-binary", b"a\0b\n\xffz")
    tabs = s.fixture("wc-tabs", b"a\tb\n")
    controls = s.fixture("wc-controls", b"a\rb\v\f\x7f\nlong-prefix\fX\n")
    invalid_multibyte = s.fixture("wc-invalid-multibyte", b"\xff\n")
    all_bytes = s.fixture("wc-all-bytes", bytes(range(256)))
    large = s.fixture("wc-large", b"streaming-row\n" * 25000)
    large_count = s.fixture("wc-large-count", b"x" * 10000000)
    wide_bytes_narrow_line = s.fixture("wc-wide-bytes-narrow-line", (b"x" * 50 + b"\n") * 3)
    missing = s.path("wc-missing")
    files0 = s.fixture("wc-files0", (text + "\0" + binary + "\0").encode())
    many_files0 = s.fixture(
        "wc-files0-many",
        ((text + "\0") * 12000).encode(),
    )
    zero_name = s.fixture("wc-zero-name", b"\0")
    zero_unusual = s.fixture("wc-zero-list\nname", b"\0")
    empty_files0 = s.fixture("wc-files0-empty", b"")
    unterminated_files0 = s.fixture("wc-files0-unterminated", (text + "\0" + binary).encode())
    unusual_name = s.fixture("wc-line\nbreak", b"name with newline\n")
    invalid_byte_name = s.fixture("wc-invalid-\udcff", b"name with invalid byte\n")
    quote_control_name = s.fixture("wc-quote'\nbreak", b"name with newline\n")
    unusual_files0 = s.fixture("wc-files0-unusual", (unusual_name + "\0").encode())
    missing_unusual = s.path("wc-missing-line\nbreak")
    missing_quote_control = s.path("wc-missing-'line\nbreak")
    unreadable = s.fixture("wc-unreadable", b"unreadable\n")
    s.write_failure("wc", [text])
    s.write_failure("wc", ["--help"])
    s.write_failure("wc", ["--version"])
    s.broken_pipe_case("wc", ["--files0-from=" + many_files0])
    s.interrupted_read_case("wc", [])
    s.symlink_dispatch("wc", ["-l", text])
    s.permission_denied_case("wc", ["-l", unreadable, text], unreadable)

    for source in ("/dev/null", empty):
        stdin_results = []
        for command in (
            [str(s.zovutils), "wc", "--total=never"],
            [str(s.gnu_dir / "wc"), "--total=never"],
        ):
            with open(source, "rb") as input_stream:
                result = subprocess.run(
                    command,
                    stdin=input_stream,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    env=s.env,
                    timeout=5,
                    check=False,
                )
            stdin_results.append((result.returncode, result.stdout, result.stderr))
        s.count += 1
        if stdin_results[0] != stdin_results[1]:
            raise AssertionError(f"wc stdin file-type behavior differs: {source}: {stdin_results!r}")

    same_path = s.path("wc-same-file")
    same_file_options = (
        [], ["-l"], ["-w"], ["-c"], ["-m"], ["-L"], ["-lwcL"],
        ["--total=always", "-c"],
    )
    for opts in same_file_options:
        for offset in (0, 2, 6):
            outcomes = []
            for command in (
                [str(s.zovutils), "wc", *opts, "-"],
                [str(s.gnu_dir / "wc"), *opts, "-"],
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
                    )
                outcomes.append((proc.returncode, proc.stderr, open(same_path, "rb").read()))
            s.count += 1
            if outcomes[0] != outcomes[1]:
                raise AssertionError(
                    f"wc shared-descriptor behavior differs: "
                    f"options={opts!r} offset={offset}: {outcomes!r}"
                )

        for input_offset in (0, 2, 6):
            for output_offset in (0, 2, 6):
                outcomes = []
                for command in (
                    [str(s.zovutils), "wc", *opts, "-"],
                    [str(s.gnu_dir / "wc"), *opts, "-"],
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
                        f"wc same-file input/output offsets differ: "
                        f"options={opts!r} input={input_offset} output={output_offset}: {outcomes!r}"
                    )

    for mask in range(1 << 5):
        flags = "".join(flag for bit, flag in enumerate("lwmcL") if mask & (1 << bit))
        args = (["-" + flags] if flags else []) + [text]
        for fixture in (text, binary, tabs, controls, invalid_multibyte, all_bytes):
            s.case("wc", [*args[:-1], fixture])

    for option in ("--lines", "--words", "--chars", "--bytes", "--max-line-length", "--total", "--files0-from"):
        for length in range(3, len(option) + 1):
            if option == "--total":
                s.case("wc", [option[:length] + "=always", text])
            elif option == "--files0-from":
                s.case("wc", [option[:length] + "=" + files0])
            else:
                s.case("wc", [option[:length], text])

    for args in (
        [text],
        ["-l", text],
        ["-L", wide_bytes_narrow_line, wide_bytes_narrow_line],
        ["-l", wide_bytes_narrow_line, wide_bytes_narrow_line],
        ["-w", wide_bytes_narrow_line, wide_bytes_narrow_line],
        ["--total=always", "-L", wide_bytes_narrow_line],
        ["-w", text],
        ["-c", binary],
        ["-m", binary],
        ["-L", no_final_newline],
        ["-L", tabs],
        ["-lcw", binary],
        ["-clwL", text],
        [text, binary],
        ["--total=always", text],
        ["--total=always"],
        ["--total=only"],
        ["--total=never"],
        ["--total=only", text],
        ["--total=never", text],
        ["-lcw", large],
        ["-c", large_count],
        ["-lcwL", large_count, text],
        ["--files0-from=" + files0],
        ["--files0-from", files0],
        ["--files0-from=-"],
        ["--files0-from=" + empty_files0],
        ["--files0-from=" + unusual_files0],
        ["--files0-from=" + unterminated_files0],
        ["--files0-from=" + zero_name],
        ["--files0-from=" + zero_unusual],
        [missing],
        [empty, text],
        [unusual_name, unusual_name],
        [invalid_byte_name],
        [quote_control_name, quote_control_name],
        [missing_unusual],
        [missing_quote_control],
        ["--", "-l"],
        [missing, "--", text, "--", "--help=x"],
        [text, "-c"],
        ["--total=auto", text, binary],
        ["--total=always", text, binary],
        ["--total=only", text, binary],
        ["--total=never", text, binary],
        ["--total=a", text],
        ["--total=au", text],
        ["--total=aut", text],
        ["--total=al", text, binary],
        ["--total=alw", text, binary],
        ["--total=o", text],
        ["--total=on", text],
        ["--total=n", text],
        ["--total=ne", text],
        ["--total=nev", text],
        ["--total", "al", text, binary],
        ["--total", "on", text],
        ["--total=invalid", text],
        ["--total=", text],
        ["--total", "always", text],
        ["--debug", text],
        ["--debug", "-l", text],
        ["--debug", "-lc", text],
        ["--debug", "-lw", text],
        ["--help"],
        ["--total"],
        ["--bytes=1"],
        ["--lines=1"],
        ["--words=1"],
        ["--chars=1"],
        ["--max-line-length=1"],
        ["--files0-from"],
        ["--files0-from", missing],
        ["--files0-from=" + files0, text],
        ["--files0-from=" + files0, unusual_name],
        ["-x"],
        ["--bogus"],
    ):
        stdin = b""
        if args == ["--files0-from=-"]:
            stdin = (text + "\0" + binary + "\0").encode()
        s.case("wc", args, stdin=stdin)
    for args, stdin in (
        ([], b"one two\nthree\n"),
        (["-mc"], b"one two\n"),
        (["-c", "-", text], b"one\ntwo\n"),
        (["-l", "-", text], b"one\ntwo\n"),
        (["-l"], b"one\ntwo"),
        (["-m"], b"a\0\xff"),
        (["-L"], b"a\tb\n"),
        (["-", "-"], b"one two\n"),
        (["--total=always"], b"one two\n"),
        (["--total=only"], b"one two\n"),
        (["--total=never"], b"one two\n"),
    ):
        s.case("wc", args, stdin=stdin)

    # The full character, whitespace, and display-width paths depend on the
    # active LC_CTYPE locale. Run them where a UTF-8 locale is installed.
    utf8_locale = None
    for candidate in ("C.UTF-8", "C.utf8", "UTF-8", "en_US.UTF-8", "en_US.utf8"):
        try:
            locale.setlocale(locale.LC_CTYPE, candidate)
            utf8_locale = candidate
            locale.setlocale(locale.LC_CTYPE, "C")
            break
        except locale.Error:
            continue
    if utf8_locale is not None:
        utf8 = {
            "LC_ALL": utf8_locale,
            "LANG": utf8_locale,
        }
        utf8_fixtures = (
            s.fixture("wc-utf8-mixed", "é\u3000中\n".encode()),
            s.fixture("wc-utf8-combining", "e\u0301x\n".encode()),
            s.fixture("wc-utf8-wide", "界界\n".encode()),
            s.fixture("wc-utf8-spaces", "alpha\u2003beta\u00a0gamma\u2007delta\u202fepsilon\u2060zeta\n".encode()),
            s.fixture("wc-utf8-invalid", b"a\xffb\n"),
            s.fixture("wc-utf8-truncated", b"a\xe2\x82"),
            s.fixture("wc-utf8-tabs", "界\tx\n".encode()),
            s.fixture("wc-utf8-boundary", b"a" * 65535 + "é\n".encode()),
            s.fixture("wc-utf8-invalid-boundary", b"a" * 65535 + b"\xe2\x82\n"),
            all_bytes,
        )
        for fixture in utf8_fixtures:
            for flags in ("-m", "-w", "-L", "-wmL"):
                s.case("wc", [flags, fixture], extra_env=utf8)
        s.case("wc", ["-wmL", utf8_fixtures[0], utf8_fixtures[2]], extra_env=utf8)
        s.case(
            "wc",
            ["-wmL"],
            stdin="é\u3000中\n界界".encode(),
            extra_env=utf8,
        )
        rng = random.Random(0x911)
        chunks = (
            b" ", b"\t", b"\n", b"\r", b"\0", b"\xff", b"\xc0", b"\xe2",
            b"\x82", b"\xf0", b"\x9f", b"\xe2\x80\x83", b"\xc3\xa9",
            b"\xe7\x95\x8c", b"\xcc\x81", b"\xe2\x80\x8b", b"\xe2\x80\xa8",
        )
        for index in range(250):
            data = bytearray()
            for _ in range(rng.randrange(80)):
                if rng.random() < 0.8:
                    data.extend(rng.choice(chunks))
                else:
                    data.append(rng.randrange(256))
            fixture = s.fixture(f"wc-utf8-random-{index}", bytes(data))
            for flags in ("-m", "-w", "-L", "-wmL", "-lmwcL"):
                s.case("wc", [flags, fixture], extra_env=utf8)
    else:
        raise AssertionError("no UTF-8 locale is available for wc parity coverage")

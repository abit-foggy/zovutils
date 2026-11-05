"""Parity cases for head."""

from support import Suite


def run(s: Suite) -> None:
    s.product_version("head")
    empty = s.fixture("head-empty", b"")
    lines = s.fixture("head-lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("head-no-final-newline", b"a\nb\nc")
    binary = s.fixture("head-binary", b"a\0b\n\xffz")
    large = s.fixture("head-large", b"row\n" * 25000)
    missing = s.path("head-missing")

    options = (
        [],
        ["-n", "2"],
        ["-n2"],
        ["-2"],
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

    for mask in range(1 << 3):
        flags = "".join(flag for bit, flag in enumerate("qvz") if mask & (1 << bit))
        args = (["-" + flags] if flags else []) + ["-n", "1", lines, binary]
        s.case("head", args)

    for args in (
        ["--help"],
        ["--hel"],
        ["-n", "1", lines, binary],
        ["-q", "-n", "1", lines, binary],
        ["-v", "-n", "1", lines, binary],
        ["-n", "1", lines, missing],
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
    s.case("head", ["-c", "4"], stdin=b"a\0b\n\xff")
    s.case("head", ["-n", "-1"], stdin=b"one\ntwo\nthree\n")
    s.case("head", ["-c", "-2"], stdin=b"a\0b\n\xff")
    s.case("head", ["-n", "0"], stdin=b"")

"""Parity cases for head."""

from support import Suite


def run(s: Suite) -> None:
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
    fixtures = (lines, no_final_newline, binary, large)
    for opts in options:
        for fixture in fixtures:
            s.case("head", [*opts, fixture])

    for args in (
        ["-n", "1", lines, binary],
        ["-q", "-n", "1", lines, binary],
        ["-v", "-n", "1", lines, binary],
        ["-n", "1", lines, missing],
        ["--", "-n"],
    ):
        s.case("head", args)
    s.case("head", ["-n", "2"], stdin=b"one\ntwo\nthree\n")
    s.case("head", ["-c", "4"], stdin=b"a\0b\n\xff")
    s.case("head", ["-n", "0"], stdin=b"")

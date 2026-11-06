"""Parity cases for paste."""

from support import Suite


def run(s: Suite) -> None:
    left = s.fixture("paste-left", b"a1\na2\na3")
    right = s.fixture("paste-right", b"b1\n")
    third = s.fixture("paste-third", b"c1\nc2\nc3\n")
    empty = s.fixture("paste-empty", b"")
    for args in (
        [],
        ["-d", ",", left, right],
        ["-d", ",|", left, right, third],
        ["-d\\t", left, right],
        ["--delimiters=\\n", left, right],
        ["-s", left],
        ["-s", "-d", ",|", left, third],
        ["-s", "-d", "", left],
        ["-z", "-d", ",", s.fixture("paste-zero-a", b"x\0y\0"), s.fixture("paste-zero-b", b"1\0" + b"2\0")],
        [left, empty, right],
        ["--", "-", left],
        [s.path("paste-missing"), left],
        ["--unknown"],
        ["-x"],
        [],
    ):
        s.case("paste", args, stdin=b"stdin-one\nstdin-two\n")

    s.case("paste", ["--help"])
    s.product_version("paste")
    s.symlink_dispatch("paste", ["-s", left])

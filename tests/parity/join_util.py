"""Parity cases for join."""

from support import Suite


def run(s: Suite) -> None:
    left = s.fixture("join-left", b"1 Alice north\n2 Bob south\n2 Bobby east\n4 Dana west\n")
    right = s.fixture("join-right", b"1 20\n2 30\n2 31\n3 40\n")
    colon_left = s.fixture("join-colon-left", b"a:alpha\nc:charlie\n")
    colon_right = s.fixture("join-colon-right", b"a:one\nc:three\nd:four\n")
    for args in (
        [left, right],
        ["-a", "1", left, right],
        ["-a", "2", left, right],
        ["-a", "1", "-a", "2", left, right],
        ["-v", "1", left, right],
        ["-v", "2", left, right],
        ["-1", "2", "-2", "2", left, right],
        ["-j", "2", left, right],
        ["-o", "0,1.2,2.2", left, right],
        ["-e", "MISSING", "-o", "0,1.2,2.2", "-a", "1", left, right],
        ["-t", ":", "-o", "1.1,2.2", colon_left, colon_right],
        ["--header", left, right],
        ["--nocheck-order", left, right],
        ["--", left, right],
        [s.path("join-absent"), right],
        ["--unknown"],
        ["-x"],
    ):
        s.case("join", args)

    s.case("join", ["-i", left, right])
    s.case("join", ["-z", s.fixture("join-zero-left", b"a one\0c three\0"), s.fixture("join-zero-right", b"a 1\0c 3\0")])
    s.case("join", ["-", right], stdin=b"1 stdin-one\n2 stdin-two\n")
    s.case("join", ["-", "-"], stdin=b"only one stream\n")
    s.case("join", ["--check-order", s.fixture("join-unsorted", b"b two\na one\n"), right])
    s.case("join", ["--help"])
    s.product_version("join")
    s.symlink_dispatch("join", [left, right])

"""Parity cases for comm."""

from support import Suite


def run(s: Suite) -> None:
    left = s.fixture("comm-left", b"a\na\nb\nd\n")
    right = s.fixture("comm-right", b"a\nc\nd\nd\n")
    unterminated_left = s.fixture("comm-unterminated-left", b"a\nb")
    unterminated_right = s.fixture("comm-unterminated-right", b"b\nc")
    for args in (
        [left, right],
        ["-1", left, right],
        ["-2", left, right],
        ["-3", left, right],
        ["-12", left, right],
        ["-13", left, right],
        ["-23", left, right],
        ["-123", left, right],
        ["--output-delimiter=::", left, right],
        ["--output-delimiter=", left, right],
        ["--total", left, right],
        ["--total", "-1", left, right],
        ["--output-delimiter=::", "--total", left, right],
        ["--check-order", left, right],
        ["--nocheck-order", left, right],
        ["--", left, right],
        [unterminated_left, unterminated_right],
        [s.path("comm-missing"), right],
        ["--unknown"],
        ["-x"],
    ):
        s.case("comm", args)

    s.case("comm", ["-z", s.fixture("comm-zero-left", b"a\0b\0d\0"), s.fixture("comm-zero-right", b"a\0c\0d\0")])
    s.case("comm", ["-", right], stdin=b"a\nb\n")
    s.case("comm", ["-", "-"], stdin=b"a\n")
    s.case("comm", ["--check-order", s.fixture("comm-unsorted", b"b\na\n"), right])
    s.case("comm", ["--help"])
    s.product_version("comm")
    s.symlink_dispatch("comm", [left, right])

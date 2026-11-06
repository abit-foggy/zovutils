"""Parity cases for seq."""

from support import Suite


def run(s: Suite) -> None:
    for args in (
        ["1"],
        ["0"],
        ["-3"],
        ["1", "4"],
        ["4", "1"],
        ["1", "2", "9"],
        ["9", "-2", "1"],
        ["1", "-1", "3"],
        ["1.0", "3.0"],
        [".5", "2"],
        ["1", "0.25", "1.25"],
        ["-w", "-2", "2"],
        ["-s", ",", "1", "3"],
        ["--separator=", "1", "3"],
        ["-f", "[%04.1f]", "1", "2"],
        ["--format=%g", "1", "0.5", "2"],
        ["--format=%% %.2f", "1", "2"],
        ["--", "-1", "2"],
        ["1e-2", "3e-2"],
        ["nan"],
        ["foo"],
        ["1", "0", "2"],
        ["-f", "%.2f", "-w", "1", "2"],
        ["--equal-width=bad", "1"],
        ["--format=%s", "1"],
        ["--format=plain", "1"],
        ["--format=%f:%f", "1"],
        ["--format=%", "1"],
        ["--unknown", "1"],
        ["1", "2", "3", "4"],
        [],
    ):
        s.case("seq", args)

    s.case("seq", ["--help"])
    s.product_version("seq")
    s.symlink_dispatch("seq", ["1", "3"])

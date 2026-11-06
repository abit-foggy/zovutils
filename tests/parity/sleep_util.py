"""Parity cases for sleep."""

from support import Suite


def run(s: Suite) -> None:
    for args in (
        ["0"],
        ["0.001"],
        ["0.001s"],
        ["0.00001m"],
        ["0.000001h"],
        ["0.0000001d"],
        ["0.001", "0.001s"],
        ["1e-3"],
        ["nan"],
        ["1x"],
        ["0.001ms"],
        ["--", "-1"],
        ["-n"],
        ["--unknown"],
        ["--help=bad"],
        ["--version=bad"],
        [],
    ):
        s.case("sleep", args)

    s.case("sleep", ["--help"])
    s.case("sleep", ["--he"])
    s.product_version("sleep")
    s.symlink_dispatch("sleep", ["0.001"])

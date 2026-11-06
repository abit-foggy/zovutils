"""Parity cases for true."""

from support import Suite


def run(s: Suite) -> None:
    for args in (
        [],
        ["ignored"],
        ["one", "two", "three"],
        ["--"],
        ["--bad-option"],
        ["-n"],
        ["--help"],
        ["--help", "ignored"],
        ["--version", "ignored"],
        ["ignored", "--help"],
    ):
        s.case("true", args)

    s.product_version("true")
    s.symlink_dispatch("true", ["ignored", "--bad-option"])

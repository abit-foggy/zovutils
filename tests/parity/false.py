"""Parity cases for false."""

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
        s.case("false", args)

    s.product_version("false", status=1)
    s.symlink_dispatch("false", ["ignored", "--bad-option"])

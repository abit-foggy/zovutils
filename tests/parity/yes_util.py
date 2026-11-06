"""Parity cases for yes."""

from support import Suite


def run(s: Suite) -> None:
    for args in (
        ["--unknown"],
        ["-n"],
        ["--help=bad"],
        ["--version=bad"],
    ):
        s.case("yes", args)

    s.case("yes", ["--help"])
    s.product_version("yes")
    s.prefix_case("yes", [], size=128)
    s.prefix_case("yes", ["hello"], size=128)
    s.prefix_case("yes", ["one", "two", "three"], size=128)
    s.prefix_case("yes", ["--", "-n"], size=128)
    s.prefix_case("yes", ["one", "--", "--unknown"], size=128)
    s.prefix_case("yes", ["yes", "line"], size=128, symlink=True)
    s.broken_pipe_case("yes", [])

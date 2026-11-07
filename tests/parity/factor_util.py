"""Parity cases for factor."""

from support import Suite


def run(s: Suite) -> None:
    for args in (["0"], ["1"], ["12"], ["-h", "360"], ["18446744073709551615"], ["abc"], ["--bad-option"]):
        s.case("factor", args)
    s.case("factor", [], stdin=b"0 1 97 100\n")
    s.product_version("factor")
    s.symlink_dispatch("factor", ["84"])

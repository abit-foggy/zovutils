"""Parity cases for factor."""

from support import Suite


def run(s: Suite) -> None:
    for args in (["0"], ["1"], ["+0012"], ["-h", "360"], ["18446744073709551615"], ["1000000016000000063"], ["18446744073709551616"], ["170141183460469231731687303715884105727"], ["340282366920938463463374607431768211455"], ["340282366920938463463374607431768211456"], ["abc"], ["--bad-option"], ["--help"]):
        s.case("factor", args)
    s.case("factor", [], stdin=b"0 1 97 100\n")
    s.case("factor", [], stdin=b"97\n100\nnot-a-number\n")
    s.case("factor", ["-h", "1000000016000000063", "360"])
    s.product_version("factor")
    s.symlink_dispatch("factor", ["84"])

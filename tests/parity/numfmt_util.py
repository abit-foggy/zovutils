"""Parity cases for numfmt."""

from support import Suite


def run(s: Suite) -> None:
    for args in (["1000"], ["--to=si", "1000"], ["--to=iec", "2048"], ["--from=si", "1k"], ["--to=iec-i", "4096"], ["--format=%.2f", "1.25"], ["--padding=8", "12"], ["bad"], ["--bad-option"]):
        s.case("numfmt", args)
    s.case("numfmt", ["--to=si"], stdin=b"1000\n2000\n")
    s.product_version("numfmt")
    s.symlink_dispatch("numfmt", ["1000"])

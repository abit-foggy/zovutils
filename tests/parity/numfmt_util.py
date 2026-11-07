"""Parity cases for numfmt."""

from support import Suite


def run(s: Suite) -> None:
    for args in (
        ["1000"], ["--to=si", "1000"], ["--to=iec", "2048"],
        ["--from=si", "1k"], ["--from=auto", "1M"], ["--from=auto", "1Mi"],
        ["--from=iec-i", "1Mi"], ["--from=iec-i", "1M"], ["--from=si", "1Ki"],
        ["--to=iec-i", "4096"],
        ["--format=%.2f", "1.25"], ["--padding=8", "12"],
        ["--format=%10.2f", "1.25"], ["--format=bad", "1"],
        ["--format=%d", "1"], ["--format=%f%g", "1"],
        ["--from-unit=2", "1.25"], ["--to-unit=10", "123.00"],
        ["--from", "si", "1K"], ["--to", "iec", "2048"],
        ["--from-unit", "2", "1.25"], ["--to-unit", "10", "123.00"],
        ["--to=si", "--round=down", "--", "-1.26"], ["--to=si", "--round=up", "999.99"],
        ["--to=si", "--round=from-zero", "999.99"], ["--to=si", "--round=towards-zero", "999.99"],
        ["--to=si", "--round=nearest", "999.99"], ["--to=si", "999.99"],
        ["--suffix=bytes", "--to=si", "1000"], ["bad"],
        ["--from=si", "--suffix=bytes", "1kbytes"],
        ["--from=si", "--suffix=bytes", "1k"],
        ["--to=si", "--unit-separator= ", "1000"],
        ["--to=si", "--grouping", "1000"], ["--to=si", "--debug", "1000"],
        ["--delimiter=ab", "1000"], ["--from-unit=0", "1"],
        ["--round=bad", "1"], ["--invalid=bad", "1"],
        ["--invalid=warn", "bad", "1000"], ["--invalid=ignore", "bad"],
        ["--invalid=fail", "bad", "1000"], ["--field=x", "1000"], ["--from=none", "1K"],
        ["+1"], ["1."], ["1e3"], ["1m"], ["1E"], [".5"],
        ["--to=auto", "1000"], ["--bad-option"],
    ):
        s.case("numfmt", args)
    s.case("numfmt", ["--to=si"], stdin=b"1000\n2000\n")
    s.case("numfmt", ["--to=iec", "--field=2"], stdin=b"  1  1024\n")
    s.case("numfmt", ["--to=si", "--delimiter=,", "--field=2"], stdin=b"a,1000,z\n")
    s.case("numfmt", ["--to=si", "--header"], stdin=b"head\n1000\n")
    s.case("numfmt", ["--to=si", "--field=2-3"], stdin=b"a 1000 2000 z\n")
    s.case("numfmt", ["--to=si", "--field=-"], stdin=b"1000 2000\n")
    s.case("numfmt", ["--to=si", "--delimiter=,", "--field=2"], stdin=b"a,,1000\n")
    s.case("numfmt", ["--to=si", "--header=2"], stdin=b"head1\nhead2\n1000\n")
    s.case("numfmt", ["--to=si", "--zero-terminated"], stdin=b"1000\0")
    s.case("numfmt", ["--to=si"], stdin=b" 1.2  x\n")
    s.product_version("numfmt")
    s.symlink_dispatch("numfmt", ["1000"])
    s.case("numfmt", ["--help"])

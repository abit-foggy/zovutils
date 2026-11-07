"""GNU parity coverage for base64."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"abc", bytes(range(256)), b"trailing bytes\0\xff"):
        s.case("base64", [], stdin=data)
    s.case("base64", ["-d"], stdin=b"YWJj\n")
    s.case("base64", ["-d", "-i"], stdin=b"Y!WJj\n")
    s.case("base64", ["-w", "0"], stdin=bytes(range(128)))
    s.case("base64", ["--bad-option"])
    s.case("base64", ["--base32"])
    s.product_version("base64")
    s.symlink_dispatch("base64", ["-w", "0"])

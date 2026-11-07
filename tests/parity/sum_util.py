"""GNU parity cases for sum."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc\n", bytes(range(256))):
        s.case("sum", [], stdin=data)
        s.case("sum", ["-s"], stdin=data)
        s.case("sum", ["-r"], stdin=data)
    path=s.fixture("sum-input", b"streamed\0bytes")
    s.case("sum", [path])
    s.case("sum", ["--bad-option"])
    s.product_version("sum")
    s.symlink_dispatch("sum", [])

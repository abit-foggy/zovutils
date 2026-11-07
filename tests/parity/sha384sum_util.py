"""GNU parity cases for sha384sum."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"final byte"):
        s.case("sha384sum", [], stdin=data)
    path=s.fixture("sha384-input", b"file\0bytes")
    s.case("sha384sum", [path])
    s.case("sha384sum", ["--bad-option"])
    s.product_version("sha384sum")
    s.symlink_dispatch("sha384sum", [])

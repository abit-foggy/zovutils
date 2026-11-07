"""GNU parity cases for sha224sum."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"no final newline"):
        s.case("sha224sum", [], stdin=data)
    path=s.fixture("sha224sum-input", b"file contents\0bytes")
    s.case("sha224sum", [path])
    s.case("sha224sum", [path, "missing-sha224sum"])
    s.case("sha224sum", ["--bad-option"])
    s.product_version("sha224sum")
    s.symlink_dispatch("sha224sum", [])

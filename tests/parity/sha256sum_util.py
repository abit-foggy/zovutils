"""GNU parity cases for sha256sum."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"no final newline"):
        s.case("sha256sum", [], stdin=data)
    path=s.fixture("sha256sum-input", b"file contents\0bytes")
    s.case("sha256sum", [path])
    s.case("sha256sum", [path, "missing-sha256sum"])
    s.case("sha256sum", ["--bad-option"])
    s.product_version("sha256sum")
    s.symlink_dispatch("sha256sum", [])

"""GNU parity cases for sha1sum."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"no final newline"):
        s.case("sha1sum", [], stdin=data)
    path=s.fixture("sha1sum-input", b"file contents\0bytes")
    s.case("sha1sum", [path])
    s.case("sha1sum", [path, "missing-sha1sum"])
    s.case("sha1sum", ["--bad-option"])
    s.product_version("sha1sum")
    s.symlink_dispatch("sha1sum", [])

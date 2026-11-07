"""GNU parity cases for sha1sum."""
from support import Suite
from hash_check_cases import add_cases

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"no final newline"):
        s.case("sha1sum", [], stdin=data)
    path=s.fixture("sha1sum-input", b"file contents\0bytes")
    s.case("sha1sum", [path])
    s.case("sha1sum", [path, "missing-sha1sum"])
    s.case("sha1sum", ["--bad-option"])
    add_cases(s, "sha1sum", "sha1", "SHA1")
    s.product_version("sha1sum")
    s.symlink_dispatch("sha1sum", [])

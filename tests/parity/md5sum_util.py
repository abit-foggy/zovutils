"""GNU parity cases for md5sum."""
from support import Suite
from hash_check_cases import add_cases

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"no final newline"):
        s.case("md5sum", [], stdin=data)
    path=s.fixture("md5sum-input", b"file contents\0bytes")
    s.case("md5sum", [path])
    s.case("md5sum", [path, "missing-md5sum"])
    s.case("md5sum", ["--bad-option"])
    add_cases(s, "md5sum", "md5", "MD5")
    s.product_version("md5sum")
    s.symlink_dispatch("md5sum", [])

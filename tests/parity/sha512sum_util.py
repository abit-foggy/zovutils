"""GNU parity cases for sha512sum."""
from support import Suite
from hash_check_cases import add_cases

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"final byte"):
        s.case("sha512sum", [], stdin=data)
    path=s.fixture("sha512-input", b"file\0bytes")
    s.case("sha512sum", [path])
    s.case("sha512sum", ["--bad-option"])
    add_cases(s, "sha512sum", "sha512", "SHA512")
    s.product_version("sha512sum")
    s.symlink_dispatch("sha512sum", [])

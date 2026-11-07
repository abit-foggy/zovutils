"""GNU parity cases for b2sum."""
from support import Suite
from hash_check_cases import add_cases

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"without final newline"):
        s.case("b2sum", [], stdin=data)
    path=s.fixture("b2sum-input", b"file bytes\0content")
    s.case("b2sum", [path])
    s.case("b2sum", ["--bad-option"])
    add_cases(s, "b2sum", "blake2b", "BLAKE2b")
    s.product_version("b2sum")
    s.symlink_dispatch("b2sum", [])

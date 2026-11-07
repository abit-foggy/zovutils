"""GNU parity cases for b2sum."""
import hashlib

from support import Suite
from hash_check_cases import add_cases

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc", bytes(range(256)), b"without final newline"):
        s.case("b2sum", [], stdin=data)
    path=s.fixture("b2sum-input", b"file bytes\0content")
    s.case("b2sum", [path])
    for bits in (8, 256, 512):
        s.case("b2sum", [f"--length={bits}"], stdin=b"digest size selection")
    s.case("b2sum", ["-l", "128", path])
    payload=b"checksum verification payload\0with bytes"
    short_hash=hashlib.blake2b(payload, digest_size=16).hexdigest()
    short_file=s.fixture("b2sum-check-short", f"{short_hash}  {s.path('b2sum-check-target')}\n".encode())
    s.fixture("b2sum-check-target", payload)
    s.case("b2sum", ["-l", "128", "--check", short_file])
    s.case("b2sum", ["--bad-option"])
    add_cases(s, "b2sum", "blake2b", "BLAKE2b")
    s.product_version("b2sum")
    s.symlink_dispatch("b2sum", [])

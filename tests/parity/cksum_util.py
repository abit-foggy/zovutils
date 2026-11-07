"""GNU parity cases for cksum."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc\n", bytes(range(256))):
        s.case("cksum", [], stdin=data)
    path=s.fixture("cksum-input", b"streamed\0bytes")
    s.case("cksum", [path])
    s.case("cksum", [path, "missing-cksum-file"])
    s.case("cksum", ["--bad-option"])
    s.product_version("cksum")
    s.symlink_dispatch("cksum", [])

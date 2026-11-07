"""GNU parity coverage for base32."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"ab", b"abc", bytes(range(64))):
        s.case("base32", [], stdin=data)
    for encoded in (b"MY======\n", b"MFRGQ===\n", b"MFRGG===\n"):
        s.case("base32", ["-d"], stdin=encoded)
    s.case("base32", ["-w", "0"], stdin=b"abc")
    s.case("base32", ["--bad-option"])
    s.case("base32", ["--base64"])
    s.product_version("base32")
    s.symlink_dispatch("base32", [])

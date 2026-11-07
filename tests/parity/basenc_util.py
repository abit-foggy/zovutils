"""GNU parity coverage for basenc supported alphabets."""
from support import Suite

def run(s: Suite) -> None:
    for option, encoded in (("--base64", b"YWJj\n"), ("--base32", b"MFRGG===\n")):
        s.case("basenc", [option], stdin=b"abc")
        s.case("basenc", [option, "-d"], stdin=encoded)
    s.case("basenc", ["--base64", "--bad-option"])
    s.product_version("basenc")
    s.symlink_dispatch("basenc", ["--base64"])

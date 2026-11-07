"""GNU parity coverage for basenc supported alphabets."""
from support import Suite

def run(s: Suite) -> None:
    for option, encoded in (("--base64", b"YWJj\n"), ("--base32", b"MFRGG===\n")):
        s.case("basenc", [option], stdin=b"abc")
        s.case("basenc", [option, "-d"], stdin=encoded)
    for option, encoded in (
        ("--base32hex", b"C5H66===\n"),
        ("--base16", b"616263"),
        ("--base2msbf", b"011000010110001001100011"),
        ("--base2lsbf", b"100001100100011011000110"),
        ("--base58", b"ZiCa\n"),
        ("--z85", b"vpA.S\n"),
    ):
        raw = b"abcd" if option == "--z85" else b"abc"
        s.case("basenc", [option], stdin=raw)
        s.case("basenc", [option, "--decode"], stdin=encoded)
    s.case("basenc", ["--base64", "--bad-option"])
    s.case("basenc", ["--z85"], stdin=b"abc")
    s.case("basenc", ["--z85", "--decode"], stdin=b"abcd")
    s.case("basenc", ["--base58", "--decode"], stdin=b"0")
    s.case("basenc", ["--base16", "--wrap=0"], stdin=bytes(range(128)))
    s.product_version("basenc")
    s.symlink_dispatch("basenc", ["--base64"])

"""GNU parity cases for ptx empty-input and version behavior."""
from support import Suite

def run(s: Suite) -> None:
    s.case("ptx", [], stdin=b"")
    s.case("ptx", ["-A"], stdin=b"")
    s.product_version("ptx")

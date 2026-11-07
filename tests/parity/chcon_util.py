"""GNU parity cases for chcon."""
from support import Suite

def run(s: Suite) -> None:
    path=s.fixture("chcon-file",b"context target")
    s.case("chcon",["system_u:object_r:bin_t:s0",path])
    s.case("chcon",["--bad-option"])
    s.product_version("chcon")

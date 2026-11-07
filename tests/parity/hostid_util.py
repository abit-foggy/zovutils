"""GNU parity cases for hostid."""
from support import Suite

def run(s: Suite) -> None:
    s.case("hostid", [])
    s.case("hostid", ["--bad-option"])
    s.product_version("hostid")
    s.symlink_dispatch("hostid", [])

"""GNU parity cases for logname."""
from support import Suite

def run(s: Suite) -> None:
    s.case("logname", [])
    s.case("logname", ["--bad-option"])
    s.product_version("logname")
    s.symlink_dispatch("logname", [])

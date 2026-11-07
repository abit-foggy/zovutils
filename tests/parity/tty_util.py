"""GNU parity cases for tty."""
from support import Suite

def run(s: Suite) -> None:
    s.case("tty", [])
    s.case("tty", ["-s"])
    s.case("tty", ["--quiet"])
    s.case("tty", ["--bad-option"])
    s.product_version("tty")
    s.symlink_dispatch("tty", ["-s"])

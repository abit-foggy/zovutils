"""GNU parity cases for who."""
from support import Suite

def run(s: Suite) -> None:
    for args in ([], ["-H"], ["-q"], ["--bad-option"]):
        s.case("who", args)
    s.product_version("who")
    s.symlink_dispatch("who", [])

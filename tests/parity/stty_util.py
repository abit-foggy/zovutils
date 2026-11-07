"""GNU parity cases for stty without a terminal."""
from support import Suite

def run(s: Suite) -> None:
    for args in ([], ["-a"], ["-g"]):
        s.case("stty", args)
    s.product_version("stty")
    s.symlink_dispatch("stty", ["-a"])

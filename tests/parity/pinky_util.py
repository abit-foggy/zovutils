"""GNU parity cases for pinky."""
from support import Suite

def run(s: Suite) -> None:
    s.case("pinky", [])
    s.case("pinky", ["-l"])
    s.case("pinky", ["--bad-option"])
    s.product_version("pinky")
    s.symlink_dispatch("pinky", [])

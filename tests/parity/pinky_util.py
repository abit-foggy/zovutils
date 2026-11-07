"""GNU parity cases for pinky."""
from support import Suite

def run(s: Suite) -> None:
    s.case("pinky", [])
    s.case("pinky", ["-l"])
    s.case("pinky", ["-l", "root"])
    s.case("pinky", ["-lb", "root"])
    s.case("pinky", ["-lbhp", "root"])
    s.case("pinky", ["-f"])
    s.case("pinky", ["-w"])
    s.case("pinky", ["-i"])
    s.case("pinky", ["-q"])
    s.case("pinky", ["root"])
    s.case("pinky", ["--bad-option"])
    s.product_version("pinky")
    s.symlink_dispatch("pinky", [])

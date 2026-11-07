"""GNU parity cases for stdbuf wrapper behavior."""
from support import Suite

def run(s: Suite) -> None:
    s.case("stdbuf", ["-o0", "sh", "-c", "printf output"])
    s.case("stdbuf", ["-o", "L", "sh", "-c", "printf output"])
    s.case("stdbuf", ["-o", "1024", "sh", "-c", "printf output"])
    s.product_version("stdbuf")

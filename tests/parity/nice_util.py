"""GNU parity cases for nice."""
from support import Suite

def run(s: Suite) -> None:
    for args in ([], ["-n", "0", "/bin/true"], ["--adjustment=0", "/bin/sh", "-c", "exit 7"], ["-n", "bad", "/bin/true"], ["--bad-option"]):
        s.case("nice", args)
    s.product_version("nice")
    s.symlink_dispatch("nice", ["-n", "0", "/bin/true"])

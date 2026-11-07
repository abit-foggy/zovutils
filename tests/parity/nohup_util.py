"""GNU parity cases for nohup."""
from support import Suite

def run(s: Suite) -> None:
    s.case("nohup", ["/bin/true"])
    s.case("nohup", ["/bin/sh", "-c", "exit 7"])
    s.case("nohup", ["--bad-option"])
    s.product_version("nohup")
    s.symlink_dispatch("nohup", ["/bin/true"])

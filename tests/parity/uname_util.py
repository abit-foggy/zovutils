"""GNU parity cases for uname."""
from support import Suite

def run(s: Suite) -> None:
    for args in ([], ["-a"], ["-s"], ["-n"], ["-r"], ["-v"], ["-m"], ["-p"], ["-i"], ["-o"], ["-snrvmo"], ["--all"], ["--bad-option"]):
        s.case("uname", args)
    s.product_version("uname")
    s.symlink_dispatch("uname", ["-s"])

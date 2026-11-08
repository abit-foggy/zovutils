"""GNU parity cases for nice."""
from pathlib import Path
from support import Suite

def run(s: Suite) -> None:
    true_cmd = "/bin/true" if Path("/bin/true").exists() else "/usr/bin/true"
    for args in ([], ["-n", "0", true_cmd], ["--adjustment=0", "/bin/sh", "-c", "exit 7"], ["-n", "bad", true_cmd], ["--bad-option"], ["-n", "0", "/nonexistent"]):
        s.case("nice", args)
    s.product_version("nice")
    s.symlink_dispatch("nice", ["-n", "0", true_cmd])

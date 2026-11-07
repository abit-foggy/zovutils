"""GNU parity cases for timeout."""
from support import Suite

def run(s: Suite) -> None:
    for args in (("0.02", "/bin/sleep", "1"), ("2", "/bin/true"), ("0.02", "-s", "KILL", "/bin/sleep", "1"), ("bad", "/bin/true"), ("--bad-option",), ("--foreground", "2", "/bin/true"), ("--kill-after=0.01", "0.02", "/bin/sleep", "1"), ("-k", "0.01", "-p", "0.02", "/bin/sleep", "1"), ("--verbose", "0.02", "/bin/sleep", "1"), ("0.02", "/bin/sh", "-c", "sleep 1 & wait")):
        s.case("timeout", list(args))
    s.product_version("timeout")
    s.symlink_dispatch("timeout", ["2", "/bin/true"])

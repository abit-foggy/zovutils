"""GNU parity cases for sync."""
from support import Suite

def run(s: Suite) -> None:
    s.case("sync", [])
    path=s.fixture("sync-file", b"persistent data")
    s.case("sync", [path])
    s.case("sync", ["-d", path])
    s.case("sync", ["--bad-option"])
    s.product_version("sync")
    s.symlink_dispatch("sync", [])

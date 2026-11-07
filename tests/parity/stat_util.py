"""GNU parity cases for stat format output."""
from support import Suite

def run(s: Suite) -> None:
    path=s.fixture("stat-file", b"stat data")
    for args in (("-c", "%n:%s:%F", path), ("--printf=%s\\n", path), ("-L", "-c", "%n:%a:%u:%g", path), ("--bad-option",)):
        s.case("stat", list(args))
    s.product_version("stat")
    s.symlink_dispatch("stat", ["-c", "%s", path])

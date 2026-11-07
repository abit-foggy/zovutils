"""GNU parity cases for date."""
from support import Suite

def run(s: Suite) -> None:
    for args in (["+%Y-%m-%d"], ["-u", "+%Y-%m-%d %H:%M"], ["--date=@0", "+%Y-%m-%d %s"], ["--bad-option"]):
        s.case("date", list(args))
    ref=s.fixture("date-reference", b"x")
    s.case("date", ["--reference", ref, "+%Y-%m-%d"])
    s.product_version("date")
    s.symlink_dispatch("date", ["+%Y"])

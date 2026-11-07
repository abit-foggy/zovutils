"""GNU parity cases for df."""
from support import Suite

def run(s: Suite) -> None:
    path=s.fixture("df-file",b"filesystem")
    s.case("df",["-P",path])
    s.case("df",["--bad-option"])
    s.product_version("df")
    s.symlink_dispatch("df",["-P",path])

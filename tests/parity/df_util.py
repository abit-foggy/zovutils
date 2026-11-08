"""GNU parity cases for df."""
from support import Suite

def run(s: Suite) -> None:
    path=s.fixture("df-file",b"filesystem")
    s.case("df",["-P",path])
    s.case("df",["-h","/"])
    s.case("df",["-H","/"])
    s.case("df",["--human-readable","/"])
    s.case("df",["--si","/"])
    s.case("df",["-i",path])
    s.case("df",["-i","/"])
    s.case("df",["-T",path])
    s.case("df",["-T","/"])
    s.case("df",["--bad-option"])
    s.product_version("df")
    s.symlink_dispatch("df",["-P",path])

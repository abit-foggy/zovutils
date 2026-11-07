"""GNU parity cases for pr's headerless path."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"",b"one\ntwo\n",b"no final newline"):
        s.case("pr",["-t"],stdin=data)
    path=s.fixture("pr-file",b"alpha\nbeta\n")
    s.case("pr",["-T",path])
    s.case("pr",["--bad-option"])
    s.product_version("pr")
    s.symlink_dispatch("pr",["-t"])

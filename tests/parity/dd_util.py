"""GNU parity cases for dd's block copy path."""
from support import Suite

def run(s: Suite) -> None:
    for args,data in ((("status=none","bs=3","count=2"),b"abcdefg"), (("status=none","bs=2","skip=1","count=2"),b"abcdef"), (("status=none","count=0"),b"input"), (("status=none","conv=notrunc","bs=1","count=2"),b"ab")):
        s.case("dd",list(args),stdin=data)
    s.case("dd",["bad=operand"])
    s.case("dd",["bs=invalid","status=none"])
    s.product_version("dd")
    s.symlink_dispatch("dd",["status=none","count=0"])

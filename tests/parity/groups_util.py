"""GNU parity cases for groups."""
import getpass
from support import Suite

def run(s: Suite) -> None:
    for args in ([], [getpass.getuser()], ["--bad-option"]):
        s.case("groups", args)
    s.product_version("groups")
    s.symlink_dispatch("groups", [])

"""GNU parity cases for dircolors configuration parsing."""
from support import Suite

def run(s: Suite) -> None:
    config=s.fixture("colors.conf",b"# sample\nDIR 01;34\n*.foo 31\nLINK 01;36\nEXEC 01;32\n")
    for args in (("-b",config),("-c",config),("--bad-option",)):
        s.case("dircolors",list(args))
    s.product_version("dircolors")
    s.symlink_dispatch("dircolors",["-b",config])

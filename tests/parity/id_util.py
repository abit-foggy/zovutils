"""GNU parity cases for id."""
from support import Suite

def run(s: Suite) -> None:
    for args in ([], ["-u"], ["-g"], ["-G"], ["-un"], ["-gn"], ["-Gn"], ["-ur"], ["-gr"], ["--bad-option"]):
        s.case("id", args)
    s.product_version("id")
    s.symlink_dispatch("id", ["-u"])

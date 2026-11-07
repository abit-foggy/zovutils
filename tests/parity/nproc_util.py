"""Parity cases for nproc."""

from support import Suite


def run(s: Suite) -> None:
    for args in ([], ["--all"], ["--ignore=0"], ["--ignore=1"], ["--ignore=999999"], ["--ignore=x"], ["--bad-option"]):
        s.case("nproc", args)
    s.product_version("nproc")
    s.symlink_dispatch("nproc", ["--all"])

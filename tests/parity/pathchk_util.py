"""Parity cases for pathchk."""

from __future__ import annotations

from support import Suite


def run(s: Suite) -> None:
    for args in ([], ["."], ["/tmp"], [""], ["-P", ""], ["-p", "simple-name"], ["-p", "has space"], ["--", "-x"], ["--bad-option"]):
        s.case("pathchk", args)
    s.product_version("pathchk")
    s.symlink_dispatch("pathchk", ["."])

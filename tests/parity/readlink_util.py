"""Parity cases for readlink."""

from __future__ import annotations

from pathlib import Path

from support import Suite


def run(s: Suite) -> None:
    target = s.root / "readlink-target"
    target.write_bytes(b"target")
    link = s.root / "readlink-link"
    link.symlink_to(target.name)
    missing = s.root / "missing-link"
    for args in (
        [str(link)], ["-n", str(link)], ["-z", str(link)], ["-q", str(missing)],
        ["-v", str(missing)], ["-f", str(link)], ["-e", str(link)],
        ["-m", str(s.root / "missing" / "child")], [], ["--bad-option"],
    ):
        s.case("readlink", args)
    s.product_version("readlink")
    s.symlink_dispatch("readlink", [str(link)])
    s.case("readlink", [str(missing)], extra_env={"POSIXLY_CORRECT": "1"})

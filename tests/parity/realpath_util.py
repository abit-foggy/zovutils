"""Parity cases for realpath."""

from __future__ import annotations

from support import Suite


def run(s: Suite) -> None:
    (s.root / "rp-existing").mkdir()
    (s.root / "rp-existing" / "child").write_text("x")
    (s.root / "rp-link").symlink_to("rp-existing")
    cases = (
        [s.path("rp-existing/child")], ["-e", s.path("rp-existing/child")], ["-e", s.path("rp-absent")],
        ["-m", s.path("rp-absent/child")], ["-s", s.path("rp-link")], ["-L", s.path("rp-link/../rp-existing")],
        [f"--relative-to={s.root}", s.path("rp-existing/child")], [f"--relative-base={s.root}", s.path("rp-existing/child")],
        ["-z", s.path("rp-existing/child")], [], ["--bad-option"],
    )
    for args in cases:
        s.case("realpath", args)
    s.product_version("realpath")
    s.symlink_dispatch("realpath", [s.path("rp-existing/child")])

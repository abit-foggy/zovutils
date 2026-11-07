"""Shared project-owned checksum verification cases."""
from __future__ import annotations

import hashlib

from support import Suite


def add_cases(s: Suite, utility: str, algorithm: str, label: str) -> None:
    payload = b"checksum verification payload\0with bytes"
    path = s.fixture(f"{utility}-check-target", payload)
    digest = hashlib.new(algorithm, payload).hexdigest()
    good = s.fixture(f"{utility}-check-good", f"{digest}  {path}\n".encode())
    bad = s.fixture(f"{utility}-check-bad", f"{'0' * len(digest)}  {path}\n".encode())
    malformed = s.fixture(f"{utility}-check-malformed", b"not a checksum line\n")
    tagged = s.fixture(f"{utility}-check-tagged", f"{label} ({path}) = {digest}\n".encode())
    missing = s.fixture(f"{utility}-check-missing", f"{digest}  missing-{utility}\n".encode())
    zero = s.fixture(f"{utility}-check-zero", f"{digest}  {path}".encode() + b"\0")
    s.case(utility, ["--tag", path])
    s.case(utility, ["--zero", path])
    s.case(utility, ["--check", good])
    s.case(utility, ["-c", "--status", good])
    s.case(utility, ["--check", "--quiet", good])
    s.case(utility, ["--check", bad])
    s.case(utility, ["--check", malformed])
    s.case(utility, ["--check", "--warn", malformed])
    s.case(utility, ["--check", tagged])
    s.case(utility, ["--check", "--ignore-missing", missing])

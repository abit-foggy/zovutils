"""Parity cases for tac byte reversal and separator handling."""

from __future__ import annotations

import random

from support import Suite


def run(s: Suite) -> None:
    for options, data in (
        ([], b""),
        ([], b"one"),
        ([], b"one\n"),
        ([], b"one\ntwo"),
        ([], b"one\ntwo\n"),
        ([], b"one\n\nthree\n"),
        (["-b"], b"one\ntwo"),
        (["-b"], b"one\ntwo\n"),
        (["-s", "--"], b"one--two--three"),
        (["-b", "-s", "--"], b"one--two--three"),
        (["--separator="], b"keep all bytes"),
        (["-s", "aba"], b"xabayabaZ"),
        (["-r", "-s", "\\n+"], b"a\n\nB\n"),
        (["-r", "-s", "[,;]"], b"a,b;c"),
        (["-r", "-s", "[0-9]+"], b"aa12bb3cc"),
        (["-b", "-r", "-s", "[0-9]+"], b"aa12bb3cc"),
        (["-r", "-s", "[a-z]+"], b"aa12bb3cc"),
        (["-r", "-s", "1[0-9]"], b"aa12bb3cc"),
        (["-r", "-s", "[[:digit:]]+"], b"aa12bb3cc"),
        ([], b"\x00a\xff\nb\x00"),
    ):
        s.case("tac", options, stdin=data)

    first = s.fixture("tac-first", b"one\ntwo")
    second = s.fixture("tac-second", b"three\nfour\n")
    s.case("tac", [first, second])
    s.case("tac", ["--", "-dash"], stdin=b"fallback\n")
    s.case("tac", ["--unknown"])
    s.case("tac", ["--separator"])
    s.case("tac", ["-r", "-s", ""])
    s.case("tac", ["-r", "-s", "["])
    s.case("tac", ["--before=x"])
    s.case("tac", ["--help"])
    s.product_version("tac")
    s.symlink_dispatch("tac", [])

    rng = random.Random(0x7AC)
    alphabet = b"abc12,;\n\x00"
    options = ([], ["-b"], ["-s", "ab"], ["-b", "-s", "ab"],
               ["-r", "-s", "[0-9]+"], ["-b", "-r", "-s", "[a-c]+"],
               ["-r", "-s", "1[0-9]"], ["-r", "-s", "."])
    for index in range(48):
        data = bytes(rng.choice(alphabet) for _ in range(rng.randrange(0, 64)))
        s.case("tac", list(options[index % len(options)]), stdin=data)

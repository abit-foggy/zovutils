"""Parity tests for od formatting and byte-range controls."""

from __future__ import annotations

import random

from support import Suite


def run(s: Suite) -> None:
    for options, data in (
        ([], b""),
        ([], b"hello\n"),
        ([], bytes(range(1, 20))),
        (["-A", "x"], bytes(range(1, 20))),
        (["-A", "d", "-t", "x1"], bytes(range(1, 20))),
        (["-A", "n", "-t", "o1"], bytes(range(1, 20))),
        (["-t", "x1"], bytes(range(1, 20))),
        (["-t", "x2"], bytes(range(1, 20))),
        (["-t", "x4"], bytes(range(1, 20))),
        (["-t", "o2"], bytes(range(1, 20))),
        (["-t", "u1"], bytes(range(1, 20))),
        (["-t", "d2"], bytes(range(1, 20))),
        (["-t", "d4"], bytes(range(1, 20))),
        (["-t", "fF"], bytes(range(1, 20))),
        (["-t", "fD"], bytes(range(1, 20))),
        (["-t", "fH"], bytes(range(1, 20))),
        (["-t", "fB"], bytes(range(1, 20))),
        (["-t", "fL"], bytes(range(1, 20))),
        (["-t", "f16"], bytes(range(1, 20))),
        (["-t", "c"], bytes(range(1, 20))),
        (["-t", "a"], bytes(range(1, 20))),
        (["-t", "x1z"], b"hello\0world\n"),
        (["-S2"], b"\0ab\0hello\0xx\0"),
        (["--strings=3"], b"\0ab\0hello\0xx\0"),
        (["-S1"], b"unterminated"),
        (["-t", "x1", "-t", "c"], bytes(range(1, 20))),
        (["-w8"], bytes(range(1, 20))),
        (["-w"], bytes(range(1, 20))),
        (["--width=8", "-t", "x1"], bytes(range(1, 20))),
        (["-j", "3", "-N", "5"], bytes(range(1, 20))),
        (["--endian=big", "-t", "x2"], bytes(range(1, 20))),
        (["--endian", "big", "-t", "x2"], bytes(range(1, 20))),
        (["-v"], b"A" * 64),
        (["-b"], bytes(range(1, 20))),
        (["-d"], bytes(range(1, 20))),
        (["-o"], bytes(range(1, 20))),
        (["-s"], bytes(range(1, 20))),
        (["-x"], bytes(range(1, 20))),
        (["-a"], bytes(range(1, 20))),
        ([], b"A" * 64),
        ([], b"\x00\x01\x7f\x80\xff"),
    ):
        s.case("od", options, stdin=data)

    first = s.fixture("od-first", b"abc")
    second = s.fixture("od-second", b"def")
    s.case("od", [first, second])
    s.case("od", [first, "+1"])
    s.case("od", [first, "2"])
    s.case("od", ["--", "-odd-file"], stdin=b"input\n")
    s.case("od", ["--unknown"])
    s.case("od", ["-A", "bad"])
    s.case("od", ["-j", "bad"])
    s.case("od", ["-N", "bad"])
    s.case("od", ["-t", "q1"])
    s.case("od", ["-t", "x16"])
    s.case("od", ["-t", "f3"])
    s.case("od", ["-t", "a2"])
    s.case("od", ["--endian=bad"])
    s.case("od", ["--endian"])
    s.case("od", ["--help=x"])
    s.case("od", ["--version=x"])
    s.case("od", ["--output-duplicates=x"])
    s.case("od", ["--traditional=x"])
    s.case("od", ["-w0"])
    s.case("od", ["--help"])
    s.product_version("od")
    s.symlink_dispatch("od", [])

    rng = random.Random(0x0D)
    alphabet = bytes(range(0, 256))
    formats = (["-t", "x1"], ["-t", "x2"], ["-t", "u1"], ["-t", "d2"],
               ["-t", "o1"], ["-t", "c"], ["-t", "a"], ["-t", "x1z"],
               ["-t", "x1", "-t", "c"], ["-A", "n", "-t", "o1"],
               ["-A", "x", "-t", "u2"], ["-t", "fH"])
    for index in range(48):
        data = bytes(rng.choice(alphabet) for _ in range(rng.randrange(0, 65)))
        s.case("od", list(formats[index % len(formats)]), stdin=data)

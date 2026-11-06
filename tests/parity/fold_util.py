"""Parity cases for fold."""

import locale

from support import Suite


def run(s: Suite) -> None:
    first = s.fixture("fold-first", b"abcdef\nlast")
    second = s.fixture("fold-second", b"12345\n")
    empty = s.fixture("fold-empty", b"")

    for args, stdin in (
        ([], b"short\n"),
        (["-w5"], b"abcdef\nabcde\n"),
        (["--width=3"], b"abcdef"),
        (["-sw5"], b"abc defgh\n"),
        (["-sw5"], b"abcdef\n"),
        (["-b2"], "é中x\n".encode()),
        (["-c1"], "é中x\n".encode()),
        (["-bc", "-w2"], "é中x\n".encode()),
        (["-cb", "-w2"], "é中x\n".encode()),
        (["-w2"], b"a\tb\n"),
        (["-w3"], b"abc"),
        (["-w3"], b"abcd"),
        (["--bytes", "--width=4"], b"abcdefgh\n"),
        (["--characters", "-w2"], b"abcd\n"),
        (["-w3"], b"a\bcd\n"),
        (["--", first, second], b"ignored\n"),
        ([first, empty, "-"], b"stdin\n"),
        ([s.path("fold-missing"), first], b""),
        (["--unknown"], b""),
        (["-x"], b""),
        (["-w0"], b""),
        (["-w", "bad"], b""),
        (["-w7"], b"x" * 100_000 + b"\n"),
    ):
        s.case("fold", args, stdin=stdin)

    s.case("fold", ["--help"])
    s.product_version("fold")
    s.symlink_dispatch("fold", ["-w3"])
    s.write_failure("fold", ["-w3"])
    try:
        locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
    except locale.Error:
        pass
    else:
        s.case("fold", ["-w3"], stdin="中éab\n".encode(), extra_env={"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"})

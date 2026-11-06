"""Parity cases for expand."""

import locale

from support import Suite


def run(s: Suite) -> None:
    first = s.fixture("expand-first", b"\talpha\nx\tbeta\n")
    second = s.fixture("expand-second", b"\tsecond\n")
    empty = s.fixture("expand-empty", b"")

    for args, stdin in (
        ([], b"\talpha\nx\tbeta\n"),
        (["-t4"], b"\talpha\nx\tbeta\n"),
        (["--tabs=3,8"], b"a\tb\tc\td\n"),
        (["-t", "3,+4"], b"a\tb\tc\td\n"),
        (["-t", "3,8,/2"], b"a\tb\tc\td\n"),
        (["-t", "4,"], b"a\tb\tc\td\n"),
        (["-t", "3,/0"], b"a\tb\tc\td\n"),
        (["-i"], b"  \tA\tB\n\tC\n"),
        (["--initial"], b"x\tA\n \tB\n"),
        ([], b"\b\tA\r\tB\n"),
        (["-t", "5"], b"12345678\tX\n"),
        ([], b"\0\tX\n"),
        (["--", first, second], b"ignored\n"),
        ([first, empty, "-"], b"\tstdin\n"),
        ([s.path("expand-missing"), first], b""),
        (["--unknown"], b""),
        (["-x"], b""),
        (["-t", "0"], b""),
        (["-t", "invalid"], b""),
        (["-t", "4,3"], b""),
    ):
        s.case("expand", args, stdin=stdin)

    s.case("expand", ["--help"])
    s.product_version("expand")
    s.symlink_dispatch("expand", ["-t4"])
    s.write_failure("expand", [])
    try:
        locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
    except locale.Error:
        pass
    else:
        s.case("expand", [], stdin="中\tX\né\tY\n".encode(), extra_env={"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"})

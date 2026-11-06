"""Parity cases for unexpand."""

import locale

from support import Suite


def run(s: Suite) -> None:
    first = s.fixture("unexpand-first", b"        alpha\n    beta\n")
    second = s.fixture("unexpand-second", b"        gamma\n")
    empty = s.fixture("unexpand-empty", b"")

    for args, stdin in (
        ([], b"        alpha\n    beta\n"),
        (["-a"], b"x       y\n  z\n"),
        (["--all"], b"x       y\n  z\n"),
        (["--first-only"], b"        x       y\n"),
        (["-t4"], b"x   y\n    z\n"),
        (["-t", "3,8"], b"        x\n"),
        (["-t", "3,+4"], b"           x\n"),
        (["-t", "3,8,/2"], b"          x\n"),
        (["-t", "4", "--first-only"], b"x   y\n    z\n"),
        (["--first-only", "-t", "4"], b"x   y\n    z\n"),
        (["-a", "--first-only"], b"x       y\n"),
        (["--first-only", "-a"], b"x       y\n"),
        ([], b"\t  leading\n"),
        (["-a"], b"a\b       b\r        c\n"),
        (["--", first, second], b"ignored\n"),
        ([first, empty, "-"], b"        stdin\n"),
        ([s.path("unexpand-missing"), first], b""),
        (["--unknown"], b""),
        (["-x"], b""),
        (["-t", "0"], b""),
        (["-t", "invalid"], b""),
        (["-t", "4,3"], b""),
    ):
        s.case("unexpand", args, stdin=stdin)

    s.case("unexpand", ["--help"])
    s.product_version("unexpand")
    s.symlink_dispatch("unexpand", ["-t4"])
    s.write_failure("unexpand", [])
    try:
        locale.setlocale(locale.LC_CTYPE, "C.UTF-8")
    except locale.Error:
        pass
    else:
        s.case("unexpand", ["-a"], stdin="中       X\n".encode(), extra_env={"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"})

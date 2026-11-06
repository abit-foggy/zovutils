"""Parity cases for cut."""

import locale

from support import Suite


def run(s: Suite) -> None:
    text = b"alpha:beta:gamma\nshort\n:empty::tail\nno-final"
    fields = b"one\ttwo\tthree\nplain\n\tblank\t\n"
    binary = b"\x00A\xffB\n\x10\x20\x30"
    first = s.fixture("cut-first", text)
    second = s.fixture("cut-second", b"x:y:z\n")
    tabbed = s.fixture("cut-tabbed", fields)
    for args, stdin in (
        (["-b", "1-3"], text),
        (["-b1,4,6-"], binary),
        (["-c", "2-4"], text),
        (["-f", "1,3", "-d", ":"], text),
        (["-f2", "-d:"], text),
        (["-f", "2-", "--output-delimiter=|"], fields),
        (["-f", "1-", "-s"], text),
        (["-f", "2", "-d", "\t"], fields),
        (["-f", "1,3", "--complement", "-d", "\t"], fields),
        (["-b", "1-", "--complement"], text),
        (["-z", "-b", "1-2"], b"ab\x00cd\x00"),
        (["-n", "-b", "1-2"], "猫\n".encode()),
    ):
        s.case("cut", args, stdin=stdin)

    for args in (
        ["-f", "1", "-d", ":", first],
        ["-f", "2", "-d", ":", first, second],
        ["-b", "1-3", first],
        ["-f", "1", "-d", "\t", tabbed],
        ["-f", "1", "-d", ":", s.path("cut-missing")],
        ["-b", "0"],
        ["-f", "-"],
        ["-b", "2-1"],
        ["-b", "1,,2"],
        ["-b", "1", "-f", "1"],
        ["-b", "1", "-d", ":"],
        ["-f", "1", "--output-delimiter=|"],
        ["-n", "-c", "1"],
        ["--unknown"],
        [],
    ):
        s.case("cut", args)

    s.product_version("cut")
    s.symlink_dispatch("cut", ["-b", "1-2"])

    utf8_locale = None
    for candidate in ("C.UTF-8", "C.utf8", "UTF-8", "en_US.UTF-8", "en_US.utf8"):
        try:
            locale.setlocale(locale.LC_CTYPE, candidate)
        except locale.Error:
            continue
        utf8_locale = candidate
        break
    if utf8_locale is not None:
        utf8 = s.fixture("cut-utf8", "aé中z\n".encode())
        env = {"LC_ALL": utf8_locale, "LANG": utf8_locale}
        s.case("cut", ["-c", "2-3", utf8], extra_env=env)
        s.case("cut", ["-b", "1-2", "-n", utf8], extra_env=env)
        s.case("cut", ["-b", "2-3", "-n", utf8], extra_env=env)

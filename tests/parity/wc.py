"""Parity cases for wc."""

from support import Suite


def run(s: Suite) -> None:
    empty = s.fixture("wc-empty", b"")
    text = s.fixture("wc-text", b"alpha\nbeta\n\ngamma\n")
    no_final_newline = s.fixture("wc-no-final-newline", b"no-final-newline")
    binary = s.fixture("wc-binary", b"a\0b\n\xffz")
    tabs = s.fixture("wc-tabs", b"a\tb\n")
    large = s.fixture("wc-large", b"streaming-row\n" * 25000)
    missing = s.path("wc-missing")
    files0 = s.fixture("wc-files0", (text + "\0" + binary + "\0").encode())
    zero_name = s.fixture("wc-zero-name", b"\0")

    for args in (
        [text],
        ["-l", text],
        ["-w", text],
        ["-c", binary],
        ["-m", binary],
        ["-L", no_final_newline],
        ["-L", tabs],
        ["-lcw", binary],
        ["-clwL", text],
        [text, binary],
        ["--total=always", text],
        ["--total=only", text],
        ["--total=never", text],
        ["-lcw", large],
        ["--files0-from=" + files0],
        ["--files0-from", files0],
        ["--files0-from=-"],
        ["--files0-from=" + zero_name],
        [missing],
        [empty, text],
    ):
        stdin = b""
        if args == ["--files0-from=-"]:
            stdin = (text + "\0" + binary + "\0").encode()
        s.case("wc", args, stdin=stdin)
    for args, stdin in (
        ([], b"one two\nthree\n"),
        (["-l"], b"one\ntwo"),
        (["-m"], b"a\0\xff"),
        (["-L"], b"a\tb\n"),
    ):
        s.case("wc", args, stdin=stdin)

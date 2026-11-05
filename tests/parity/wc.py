"""Parity cases for wc."""

from support import Suite


def run(s: Suite) -> None:
    s.product_version("wc")
    empty = s.fixture("wc-empty", b"")
    text = s.fixture("wc-text", b"alpha\nbeta\n\ngamma\n")
    no_final_newline = s.fixture("wc-no-final-newline", b"no-final-newline")
    binary = s.fixture("wc-binary", b"a\0b\n\xffz")
    tabs = s.fixture("wc-tabs", b"a\tb\n")
    controls = s.fixture("wc-controls", b"a\rb\v\f\x7f\nlong-prefix\fX\n")
    invalid_multibyte = s.fixture("wc-invalid-multibyte", b"\xff\n")
    all_bytes = s.fixture("wc-all-bytes", bytes(range(256)))
    large = s.fixture("wc-large", b"streaming-row\n" * 25000)
    missing = s.path("wc-missing")
    files0 = s.fixture("wc-files0", (text + "\0" + binary + "\0").encode())
    zero_name = s.fixture("wc-zero-name", b"\0")
    empty_files0 = s.fixture("wc-files0-empty", b"")
    unterminated_files0 = s.fixture("wc-files0-unterminated", (text + "\0" + binary).encode())

    for mask in range(1 << 5):
        flags = "".join(flag for bit, flag in enumerate("lwmcL") if mask & (1 << bit))
        args = (["-" + flags] if flags else []) + [text]
        for fixture in (text, binary, tabs, controls, invalid_multibyte, all_bytes):
            s.case("wc", [*args[:-1], fixture])

    for option in ("--lines", "--words", "--chars", "--bytes", "--max-line-length", "--total", "--files0-from"):
        for length in range(3, len(option) + 1):
            if option == "--total":
                s.case("wc", [option[:length] + "=always", text])
            elif option == "--files0-from":
                s.case("wc", [option[:length] + "=" + files0])
            else:
                s.case("wc", [option[:length], text])

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
        ["--files0-from=" + empty_files0],
        ["--files0-from=" + unterminated_files0],
        ["--files0-from=" + zero_name],
        [missing],
        [empty, text],
        ["--", "-l"],
        [text, "-c"],
        ["--total=auto", text, binary],
        ["--total=always", text, binary],
        ["--total=only", text, binary],
        ["--total=never", text, binary],
        ["--total=invalid", text],
        ["--total=", text],
        ["--total", "always", text],
        ["--debug", text],
        ["--help"],
        ["--total"],
        ["--bytes=1"],
        ["--lines=1"],
        ["--words=1"],
        ["--chars=1"],
        ["--max-line-length=1"],
        ["--files0-from"],
        ["--files0-from", missing],
        ["--files0-from=" + files0, text],
        ["-x"],
        ["--bogus"],
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

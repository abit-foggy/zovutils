"""Parity cases for cat."""

from support import Suite


def run(s: Suite) -> None:
    empty = s.fixture("empty", b"")
    text = s.fixture("text", b"alpha\nbeta\n\ngamma\n")
    lines = s.fixture("lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("no-final-newline", b"no-final-newline")
    binary = s.fixture("binary", b"a\0b\n\xffz")
    tabs = s.fixture("tabs", b"a\tb\n\t\n")
    all_bytes = s.fixture("all-bytes", bytes(range(256)))
    boundary = s.fixture("boundary", b"x" * 65535 + b"\n\n\nend")
    large = s.fixture("large", (b"stream\0bytes\xff\n" * 131072) + b"tail")
    split_first = s.fixture("split-first", b"first")
    split_second = s.fixture("split-second", b"second\n\n")
    split_blank = s.fixture("split-blank", b"\nlast\n")
    directory = s.path("directory")
    s.root.joinpath("directory").mkdir()

    # Exercise every combination of cat's independent display/numbering flags
    # against text, binary, and chunk-boundary inputs.
    switches = "nbsETv"
    for mask in range(1 << len(switches)):
        selected = "".join(flag for bit, flag in enumerate(switches) if mask & (1 << bit))
        opts = ["-" + selected] if selected else []
        for fixture in (text, binary, boundary):
            s.case("cat", [*opts, fixture])

    # GNU accepts every unambiguous abbreviation of a long option and reports
    # the full candidate list for ambiguous prefixes.
    for option in (
        "--number",
        "--number-nonblank",
        "--squeeze-blank",
        "--show-all",
        "--show-ends",
        "--show-tabs",
        "--show-nonprinting",
    ):
        for length in range(3, len(option) + 1):
            s.case("cat", [option[:length], text])

    for args in (
        ["--help"],
        ["--h"],
        ["--he"],
        ["--hel"],
        [],
        [text],
        ["-n", text],
        ["-b", text],
        ["-n", "-b", text],
        ["-s", text],
        ["-n", "-s", text],
        ["-A", binary],
        ["-v", binary],
        ["-E", no_final_newline],
        ["-T", tabs],
        ["-t", tabs],
        ["-e", binary],
        ["-u", text],
        ["--number", text],
        ["--number-nonblank", text],
        ["--squeeze-blank", text],
        ["--show-ends", text],
        ["--show-tabs", tabs],
        ["--show-nonprinting", all_bytes],
        ["--show-all", all_bytes],
        ["--show-a", all_bytes],
        ["--show-e", all_bytes],
        ["--show-t", tabs],
        ["--show-n", all_bytes],
        ["--squeez", text],
        ["--number-n", text],
        ["--n"],
        ["--s"],
        ["-v", all_bytes],
        ["-A", all_bytes],
        ["-e", all_bytes],
        ["-t", all_bytes],
        ["-A", "-n", "-s", all_bytes],
        ["-n", "-b", "-n", text],
        ["-n", "-b", "-b", "-n", text],
        ["-b", "-s", text],
        ["-n", boundary],
        ["-s", boundary],
        [large],
        ["-A", "-e", all_bytes],
        ["-A", "-t", all_bytes],
        ["-e", "-t", all_bytes],
        ["-A", "-v", all_bytes],
        ["-u", "-A", all_bytes],
        ["-u", "-b", "-n", text],
        ["-n", "-b", "-s", "-E", "-T", "-v", boundary],
        [text, "-n"],
        [text, lines],
        ["-s", split_second, split_blank],
        ["-n", split_first, split_second],
        [split_first, "-", split_second],
        ["--", "-n"],
        [directory],
        [s.path("missing")],
        [text, s.path("missing"), lines],
        [empty, text],
        ["-x"],
        ["--bogus"],
        ["--show-all=bad"],
        ["--number="],
        ["--number-nonblank="],
        ["--squeeze-blank="],
        ["--show-all="],
        ["--show-ends="],
        ["--show-tabs="],
        ["--show-nonprinting="],
        ["--help=x"],
        ["--hel=x"],
        ["--version=x"],
        ["--ver=x"],
        ["--num=x"],
        ["--nu=x"],
        ["--n=x"],
        ["--s=x"],
        ["--show=x"],
        ["--show-a=x"],
        ["--squeeze=x"],
        ["--squeez=x"],
        ["--number-n=x"],
        ["--show-e=x"],
        ["--show-t=x"],
        ["--show-non=x"],
        ["--bogus=x"],
    ):
        s.case("cat", args, stdin=b"from-stdin\n")
    s.case("cat", [], stdin=b"stdin\0bytes\n")

    # GNU's cat behavior through an installed command-name symlink.
    alias = s.root / "cat"
    alias.symlink_to(s.zovutils)
    zov = s._run([str(alias), "-n", text], b"")
    gnu = s._run([str(s.gnu_dir / "cat"), "-n", text], b"")
    s.count += 1
    if zov != gnu:
        raise AssertionError("cat argv[0] symlink dispatch differs from GNU")

    s.product_version("cat")
    s.write_failure("cat", [text])
    s.write_failure("cat", ["--help"])
    s.write_failure("cat", ["--version"])

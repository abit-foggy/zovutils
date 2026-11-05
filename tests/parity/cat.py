"""Parity cases for cat."""

from support import Suite


def run(s: Suite) -> None:
    empty = s.fixture("empty", b"")
    text = s.fixture("text", b"alpha\nbeta\n\ngamma\n")
    lines = s.fixture("lines", b"one\ntwo\nthree\nfour\n")
    no_final_newline = s.fixture("no-final-newline", b"no-final-newline")
    binary = s.fixture("binary", b"a\0b\n\xffz")
    tabs = s.fixture("tabs", b"a\tb\n\t\n")
    directory = s.path("directory")
    s.root.joinpath("directory").mkdir()

    for args in (
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
        [text, "-n"],
        [text, lines],
        ["--", "-n"],
        [directory],
        [s.path("missing")],
        [text, s.path("missing"), lines],
        [empty, text],
    ):
        s.case("cat", args)
    s.case("cat", [], stdin=b"stdin\0bytes\n")

    # GNU's cat behavior through an installed command-name symlink.
    alias = s.root / "cat"
    alias.symlink_to(s.zovutils)
    zov = s._run([str(alias), "-n", text], b"")
    gnu = s._run([str(s.gnu_dir / "cat"), "-n", text], b"")
    s.count += 1
    if zov != gnu:
        raise AssertionError("cat argv[0] symlink dispatch differs from GNU")

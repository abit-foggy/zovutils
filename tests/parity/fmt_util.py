"""Parity cases for fmt."""

from support import Suite


def run(s: Suite) -> None:
    first = s.fixture("fmt-first", b"one two three four\nfive six\n")
    second = s.fixture("fmt-second", b"another paragraph here\n")
    empty = s.fixture("fmt-empty", b"")

    for args, stdin in (
        ([], b"one two three\nfour five six\n"),
        (["-w20"], b"one two three four five six seven\n"),
        (["-15"], b"one two three four five six\n"),
        (["-g10", "-w15"], b"one two three four five six\n"),
        (["-s", "-w10"], b"one two three four five six\n"),
        (["-u"], b"one  two   three\n"),
        (["-u", "-w20"], b"A sentence.  Another sentence!\n"),
        (["-p", "> ", "-w16"], b"> one two three four five\n> six seven eight\n>\n> next para\n"),
        (["-c", "-w10"], b"  abc def\n    ghi jkl\n"),
        (["-t", "-w10"], b"* abc def\n  ghi jkl\n"),
        ([], b"first paragraph\n\n\nsecond paragraph\n"),
        (["-s", "-w6"], b"abcde fghij\nshort line\n"),
        (["--", first, second], b"ignored\n"),
        ([first, empty, "-"], b"stdin text\n"),
        ([s.path("fmt-missing"), first], b""),
        (["--unknown"], b""),
        (["-x"], b""),
        (["-w0"], b""),
    ):
        s.case("fmt", args, stdin=stdin)

    s.case("fmt", ["--help"])
    s.product_version("fmt")
    s.symlink_dispatch("fmt", ["-w8"])
    s.write_failure("fmt", ["-w8"])

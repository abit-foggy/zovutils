"""Parity cases for nl."""

from support import Suite


def run(s: Suite) -> None:
    first = s.fixture("nl-first", b"first\n\nthird")
    second = s.fixture("nl-second", b"next\n")
    empty = s.fixture("nl-empty", b"")
    sections = b"\\:\\:\\:\nhead\n\\:\nbody\n\\:\\:\nfoot\n\\:\nlast\n"

    for args, stdin in (
        ([], b"first\n\nthird"),
        (["-ba"], b"a\n\n\nb\n"),
        (["-bn"], b"a\n\nb\n"),
        (["-bpfoo"], b"foo\nbar\nfood\n"),
        (["-l2", "-ba"], b"a\n\n\nb\n"),
        (["-nln", "-s:", "-v7", "-i2", "-w3"], b"a\nb\n"),
        (["-nrz", "-v8", "-w4"], b"a\n"),
        (["-i0", "-l0", "-v0"], b"a\n\n\nb\n"),
        (["--body-numbering=a", "--header-numbering=a", "--footer-numbering=a"], sections),
        (["-ha", "-fa", "-ba"], sections),
        (["-ha", "-fa", "-ba", "-p"], sections),
        (["-d", "<>"], b"<><><>\nheader\n<>\nbody\n<><>\nfooter\n<>\n"),
        (["--", first, second], b"ignored\n"),
        ([first, empty, "-"], b"stdin\n"),
        ([s.path("nl-missing"), first], b""),
        (["--unknown"], b""),
        (["-x"], b""),
    ):
        s.case("nl", args, stdin=stdin)

    s.case("nl", ["--help"])
    s.product_version("nl")
    s.symlink_dispatch("nl", ["-ba"])
    s.write_failure("nl", [])

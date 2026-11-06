"""Parity cases for tr."""

from support import Suite


def run(s: Suite) -> None:
    for args, stdin in (
        (["a-z", "A-Z"], b"hello, World!\n"),
        (["-c", "aeiou", "*"], b"abc xyz\n"),
        (["-d", "aeiou"], b"beautiful\n"),
        (["-s", " "], b"a  b   c\n"),
        (["-s", "a-z"], b"aabbcc\n"),
        (["-t", "abc", "xy"], b"abcxyz\n"),
        (["abc", "xy"], b"abcxyz\n"),
        (["[:lower:]", "[:upper:]"], b"Abc xyz\n"),
        ([r"\n", "X"], b"a\nb\n"),
        (["-d", "[:digit:]"], b"a1b2c3\n"),
        (["-ds", " ", "x"], b"a  xx  b\n"),
        (["abc", "[x*]"], b"abcabc\n"),
        ([r"\141-\143", r"\170-\172"], b"abc\n"),
        (["-c", "a", "X"], b"abc\n"),
    ):
        s.case("tr", args, stdin=stdin)

    s.case("tr", ["--help"])
    s.product_version("tr")
    s.symlink_dispatch("tr", ["a-z", "A-Z"])

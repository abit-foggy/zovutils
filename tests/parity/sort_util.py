"""Parity cases for sort."""

from support import Suite


def run(s: Suite) -> None:
    for args, stdin in (
        ([], b"pear\napple\nbanana\n"),
        (["-r"], b"pear\napple\nbanana\n"),
        (["-u"], b"b\na\na\nc\n"),
        (["-fu"], b"Alpha\nalpha\nBeta\nBETA\n"),
        (["-d"], b"a-b\nab\na b\n"),
        (["-b"], b"  b\na\n b\n"),
        (["-n"], b"10\n2\n-1\n0\n"),
        (["-g"], b"nan\ninf\n-2.5\n10\n"),
        (["-M"], b"Dec\nfoo\nJan\nMar\nFeb\n"),
        (["-i"], b"a\x01\nb\nA\n"),
        (["-z"], b"pear\0apple\0banana\0"),
        (["-s"], b"same\nsame\n"),
        (["--sort=numeric", "--unique"], b"01\n1\n2\n"),
        (["-t:", "-k2,2n"], b"b:10\na:2\nc:1\n"),
        (["-k1,1"], b"z 2\na 10\na 2\n"),
    ):
        s.case("sort", args, stdin=stdin)

    first = s.fixture("sort-first", b"c\na\n")
    second = s.fixture("sort-second", b"b\nd")
    s.case("sort", [first, second])
    s.case("sort", ["--", "-"], stdin=b"z\ny\n")
    s.case("sort", ["--unknown"])
    s.case("sort", ["--help"])
    s.product_version("sort")
    s.symlink_dispatch("sort", [])

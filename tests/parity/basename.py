"""Parity cases for basename."""

from support import Suite


def run(s: Suite) -> None:
    # Path trimming, including empty names and paths made only of slashes.
    for name in (
        "plain",
        "dir/name",
        "/usr/bin/sort",
        "dir///name///",
        "/",
        "////",
        "",
        ".",
        "..",
        "dir//./name",
        "café/猫",
    ):
        s.case("basename", [name])

    # The POSIX operand suffix and GNU's multiple-name forms.
    for args in (
        ["include/stdio.h", ".h"],
        ["include/stdio.h", ""],
        ["same", "same"],
        ["name.ext", ".other"],
        ["-a", "one/a", "two/b"],
        ["--multiple", "one/a", "two/b"],
        ["-s", ".h", "include/a.h", "include/b.h"],
        ["-s.h", "include/a.h", "include/b.h"],
        ["--suffix=.h", "include/a.h", "include/b.h"],
        ["-z", "one/a", "two/b"],
        ["-az", "one/a", "two/b"],
        ["--multiple", "one/a", "--zero", "two/b"],
        ["--suffix", ".h", "include/a.h", "include/b.h"],
        ["--s", ".h", "include/a.h"],
        ["--m", "one/a", "two/b"],
        ["--zero", "one/a"],
        ["--", "--leading-dash"],
    ):
        s.case("basename", args)

    # GNU basename stops parsing options after the first operand.
    s.case("basename", ["first/a", "-z", "second/b"])

    # Exact GNU diagnostics and static usage text.
    for args in (
        [],
        ["one", "two", "three"],
        ["--bad"],
        ["--multiple=value", "name"],
        ["--zero=value", "name"],
        ["--help=value"],
        ["--suffix"],
        ["-s"],
        ["-h"],
        ["-V"],
        ["-x"],
    ):
        s.case("basename", args)
    s.case("basename", ["--help"])
    s.product_version("basename")
    s.write_failure("basename", ["--help"])

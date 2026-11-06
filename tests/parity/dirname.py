"""Parity cases for dirname."""

from support import Suite


def run(s: Suite) -> None:
    for name in (
        "plain",
        "dir/name",
        "/usr/bin/sort",
        "dir///name///",
        "/",
        "//",
        "////",
        "",
        "foo/",
        "foo//",
        "/foo",
        "//foo",
        "/foo/bar",
        "//foo/bar",
        "///foo/bar",
        "a///b///c",
        ".",
        "..",
        "foo/..",
        "café/猫",
    ):
        s.case("dirname", [name])

    for args in (
        ["one/a", "two/b", "three/c"],
        ["-z", "one/a", "two/b"],
        ["--zero", "one/a", "two/b"],
        ["--z", "one/a", "two/b"],
        ["-zz", "one/a", "two/b"],
        ["--", "--leading-dash/file"],
        ["one/a", "-z", "two/b"],
    ):
        s.case("dirname", args)
    s.case(
        "dirname",
        ["one/a", "-z", "two/b"],
        extra_env={"POSIXLY_CORRECT": "1"},
    )

    for args in (
        [],
        ["--bad"],
        ["--zero=value"],
        ["--help=value"],
        ["--version=value"],
        ["-h"],
        ["-V"],
        ["-x"],
    ):
        s.case("dirname", args)
    s.case("dirname", ["--help"])
    s.product_version("dirname")
    s.write_failure("dirname", ["--help"])

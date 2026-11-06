"""Parity cases for test and the [ alias."""

from pathlib import Path

from support import Suite


def run(s: Suite) -> None:
    present = s.fixture("test-present", b"content\n")
    empty = s.fixture("test-empty", b"")
    directory = Path(s.path("test-directory"))
    directory.mkdir()
    absent = s.path("test-absent")

    for args in (
        [],
        ["x"],
        [""],
        ["!"],
        ["!", "x"],
        ["--help"],
        ["--version"],
        ["-n", "x"],
        ["-z", ""],
        ["-z", "x"],
        ["x", "=", "x"],
        ["x", "!=", "y"],
        ["x", "<", "y"],
        ["y", ">", "x"],
        ["1", "-eq", "1"],
        ["1", "-ne", "2"],
        ["2", "-gt", "1"],
        ["2", "-ge", "2"],
        ["1", "-lt", "2"],
        ["2", "-le", "2"],
        ["-e", present],
        ["-f", present],
        ["-s", present],
        ["-z", present],
        ["-e", absent],
        ["-f", directory.as_posix()],
        ["-d", directory.as_posix()],
        ["-s", empty],
        ["-r", present],
        ["-w", present],
        ["-x", present],
        ["-t", "0"],
        ["x", "-a", "y"],
        ["", "-o", "y"],
        ["!", "x", "-o", "y"],
        ["(", "x", ")"],
        ["(", "", ")"],
        ["(", "x", "-a", "y", ")", "-o", "z"],
        ["foo", "-eq", "bar"],
        ["1", "-eq", "bar"],
    ):
        s.case("test", args)

    s.case("[", ["x", "=", "x", "]"])
    s.case("[", ["x", "]"])
    s.case("[", ["x"])
    s.symlink_dispatch("[", ["x", "]"])

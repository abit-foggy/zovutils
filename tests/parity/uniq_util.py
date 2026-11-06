"""Parity cases for uniq."""

from pathlib import Path

from support import Suite


def run(s: Suite) -> None:
    records = b"a\na\nb\nA\nA\nx1\ny1\nx2\n"
    for args, stdin in (
        ([], records),
        (["-c"], b"a\na\na\nb\n"),
        (["-d"], b"a\na\nb\n"),
        (["-D"], b"a\na\nb\nc\nc\n"),
        (["-u"], b"a\na\nb\nc\nc\n"),
        (["-i"], b"a\nA\nb\n"),
        (["-f", "1"], b"x one\ny one\na two\n"),
        (["-s", "1"], b"ab\nac\nax\nby\n"),
        (["-w", "1"], b"apple\napricot\nbanana\n"),
        (["-z"], b"a\0a\0b\0"),
        (["--skip-fields=1", "--skip-chars=1"], b"a x1\nb x1\nc x2\n"),
        (["-D", "-i"], b"A\na\nb\n"),
    ):
        s.case("uniq", args, stdin=stdin)

    fixture = s.fixture("uniq-input", b"b\nb\na\n")
    s.case("uniq", [fixture])
    zov_output = s.path("uniq-zov-output")
    gnu_output = s.path("uniq-gnu-output")
    gnu_env = s.env.copy()
    gnu_env["PATH"] = str(s.gnu_dir) + ":" + gnu_env.get("PATH", "")
    zov_result = s._run([str(s.zovutils), "uniq", fixture, zov_output], b"")
    gnu_result = s._run(["uniq", fixture, gnu_output], b"", env=gnu_env)
    s.count += 1
    if zov_result != gnu_result or Path(zov_output).read_bytes() != Path(gnu_output).read_bytes():
        raise AssertionError(f"uniq output-file behavior differs: {zov_result!r} != {gnu_result!r}")
    s.case("uniq", ["--help"])
    s.case("uniq", ["--unknown"])
    s.product_version("uniq")
    s.symlink_dispatch("uniq", [])

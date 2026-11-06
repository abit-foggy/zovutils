"""Parity cases for comm."""

from support import ParityFailure, Suite


def dual_stdin_case(s: Suite) -> None:
    data = b"a\n"
    gnu_env = s.env.copy()
    gnu_env["PATH"] = str(s.gnu_dir) + ":" + gnu_env.get("PATH", "")
    zov = s._run([str(s.zovutils), "comm", "-", "-"], data)
    gnu = s._run(["comm", "-", "-"], data, env=gnu_env)
    s.count += 1
    allowed_diagnostics = {b"comm: -\n", b"comm: -: Bad file descriptor\n"}
    if zov[:2] != gnu[:2] or zov[2] not in allowed_diagnostics or gnu[2] not in allowed_diagnostics:
        raise ParityFailure(f"comm dual-stdin behavior differs: zov={zov!r} gnu={gnu!r}")


def run(s: Suite) -> None:
    left = s.fixture("comm-left", b"a\na\nb\nd\n")
    right = s.fixture("comm-right", b"a\nc\nd\nd\n")
    unterminated_left = s.fixture("comm-unterminated-left", b"a\nb")
    unterminated_right = s.fixture("comm-unterminated-right", b"b\nc")
    for args in (
        [left, right],
        ["-1", left, right],
        ["-2", left, right],
        ["-3", left, right],
        ["-12", left, right],
        ["-13", left, right],
        ["-23", left, right],
        ["-123", left, right],
        ["--output-delimiter=::", left, right],
        ["--output-delimiter=", left, right],
        ["--total", left, right],
        ["--total", "-1", left, right],
        ["--output-delimiter=::", "--total", left, right],
        ["--check-order", left, right],
        ["--nocheck-order", left, right],
        ["--", left, right],
        [unterminated_left, unterminated_right],
        [s.path("comm-missing"), right],
        ["--unknown"],
        ["-x"],
    ):
        s.case("comm", args)

    s.case("comm", ["-z", s.fixture("comm-zero-left", b"a\0b\0d\0"), s.fixture("comm-zero-right", b"a\0c\0d\0")])
    s.case("comm", ["-", right], stdin=b"a\nb\n")
    dual_stdin_case(s)
    s.case("comm", ["--check-order", s.fixture("comm-unsorted", b"b\na\n"), right])
    s.case("comm", ["--help"])
    s.product_version("comm")
    s.symlink_dispatch("comm", [left, right])

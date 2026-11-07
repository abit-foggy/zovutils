"""GNU parity cases for common ptx permuted-index behavior."""
from support import Suite


def run(s: Suite) -> None:
    sample = b"alpha beta\n"
    for args in ([], ["-A"], ["-R"], ["-A", "-R"], ["-w", "40"], ["-A", "-w", "40"], ["-g", "1"], ["-f"], ["-r"], ["-A", "-r"], ["-G"], ["-A", "-O"], ["-A", "-T"], ["-A", "-M", "XX", "-O"], ["-A", "-W", "[a-z]+"], ["-A", "-W", "beta"], ["-A", "-S", "[.!?]"]):
        s.case("ptx", args, stdin=sample)
    s.case("ptx", [], stdin=b"")
    s.case("ptx", [], stdin=b"one\ntwo\n")
    s.case("ptx", ["-A"], stdin=b"one\ntwo\n")
    s.case("ptx", ["-r"], stdin=b"alpha beta\n")
    ignored = s.fixture("ptx-ignore", b"alpha\n")
    only = s.fixture("ptx-only", b"beta\n")
    s.case("ptx", ["-A", "-i", ignored], stdin=sample)
    s.case("ptx", ["-A", "-o", only], stdin=sample)
    file = s.fixture("ptx-input", sample)
    s.case("ptx", ["-A", file])
    s.case("ptx", ["-A"], stdin=b"foo.bar alpha-beta\n")
    breaks = s.fixture("ptx-breaks", b".")
    s.case("ptx", ["-A", "--break-file", breaks], stdin=b"foo.bar\n")
    s.case("ptx", ["--bad-option"])
    s.product_version("ptx")

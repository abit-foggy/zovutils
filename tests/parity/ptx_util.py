"""GNU parity cases for common ptx permuted-index behavior."""
from support import Suite


def run(s: Suite) -> None:
    sample = b"alpha beta\n"
    for args in ([], ["-A"], ["-R"], ["-A", "-R"], ["-w", "40"], ["-A", "-w", "40"], ["-g", "1"], ["-f"], ["-r"], ["-A", "-r"], ["-G"], ["-A", "-O"], ["-A", "-T"], ["-A", "-M", "XX", "-O"], ["-A", "-W", "[a-z]+"], ["-A", "-W", "beta"], ["-A", "-S", "[.!?]"]):
        s.case("ptx", args, stdin=sample)
    # GNU long forms and option interactions that affect generated references.
    s.case("ptx", ["--auto-reference", "--right-side-refs", "--width=48"], stdin=b"alpha beta gamma\n")
    s.case("ptx", ["--auto-reference", "--flag-truncation=...", "--width=1"], stdin=b"alpha beta gamma delta epsilon\n")
    s.case("ptx", ["--traditional", "--format=roff", "--macro-name=ZZ"], stdin=sample)
    s.case("ptx", ["--typeset-mode", "--gap-size=1", "--ignore-case"], stdin=b"Alpha alpha BETA beta\n")
    s.case("ptx", ["--references", "--auto-reference"], stdin=b"ref alpha beta\n")
    s.case("ptx", ["--word-regexp=[a-z]+", "--sentence-regexp=[.!?]"], stdin=b"Alpha beta. Gamma delta!\n")
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
    s.case("ptx", ["-A", "--sentence-regexp=[.!?]"], stdin=b"one alpha. beta two! gamma three\n")
    s.case("ptx", ["--bad-option"])
    for args in (("-F",), ("--macro-name",), ("--width", "bogus"), ("--gap-size=-1",), ("--format=bad",), ("--word-regexp=[",), ("--sentence-regexp=[",)):
        s.case("ptx", list(args), stdin=sample)
    s.product_version("ptx")

"""GNU parity cases for tee."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"hello\n", bytes(range(256)), b"no final newline"):
        s.case("tee", [], stdin=data)
    s.case("tee", ["-a", s.path("tee-output")], stdin=b"append\n")
    s.case("tee", ["--", s.path("--tee-literal")], stdin=b"literal\n")
    s.case("tee", ["--bad-option"])
    s.product_version("tee")
    s.symlink_dispatch("tee", [])

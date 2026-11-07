"""GNU parity cases for tsort."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a b\nb c\nd c\n", b"one two two three\n", b"duplicate edge\nduplicate edge\n", b"a b\nb a\n", b"a b c"):
        s.case("tsort", [], stdin=data)
    path=s.fixture("tsort-graph", b"first second\nsecond third\n")
    s.case("tsort", [path])
    s.case("tsort", ["--bad-option"])
    s.product_version("tsort")
    s.symlink_dispatch("tsort", [])

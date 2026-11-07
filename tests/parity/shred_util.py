"""GNU parity cases for shred using isolated temporary files."""
from support import Suite


def run(s: Suite) -> None:
    def setup(root):
        (root / "payload").write_bytes(b"temporary contents\0")
        (root / "random").write_bytes(bytes(range(256)) * 64)

    for args in (
        ["--iterations=0", "--zero", "payload"],
        ["--iterations=1", "--random-source=/dev/zero", "payload"],
        ["--size=4", "--iterations=0", "--zero", "payload"],
        ["--exact", "--iterations=0", "--zero", "payload"],
        ["--iterations=0", "--zero", "--remove", "payload"],
    ):
        s.case_filesystem("shred", args, setup=setup)
    s.case("shred", ["--bad-option"])
    s.product_version("shred")

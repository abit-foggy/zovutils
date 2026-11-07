"""GNU parity cases for cksum."""
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"a", b"abc\n", bytes(range(256))):
        s.case("cksum", [], stdin=data)
    path=s.fixture("cksum-input", b"streamed\0bytes")
    s.case("cksum", [path])
    s.case("cksum", [path, "missing-cksum-file"])
    # GNU's cksum exposes the legacy BSD and System V algorithms as well as
    # its POSIX CRC and the reflected CRC32B variant.
    for algorithm in ("crc", "bsd", "sysv", "crc32b"):
        s.case("cksum", [f"--algorithm={algorithm}"], stdin=b"streamed\0bytes")
    s.case("cksum", ["-a", "sysv", path])
    s.case("cksum", ["-a", "bsd", path])
    s.case("cksum", ["--bad-option"])
    s.case("cksum", ["-s"])
    s.case("cksum", ["-r"])
    s.product_version("cksum")
    s.symlink_dispatch("cksum", [])

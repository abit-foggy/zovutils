"""GNU parity cases for chcon."""
from support import Suite

def run(s: Suite) -> None:
    if (s.gnu_dir / "chcon").is_file():
        path=s.fixture("chcon-file",b"context target")
        s.case("chcon",["system_u:object_r:bin_t:s0",path])
        s.case("chcon",["--bad-option"])
    else:
        print("[SKIP] chcon GNU oracle unavailable on this host (SELinux not built)")
    s.product_version("chcon")

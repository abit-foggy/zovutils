"""GNU parity cases for runcon."""
from support import Suite

def run(s: Suite) -> None:
    if (s.gnu_dir / "runcon").is_file():
        for args in (("invalid_context","/bin/true"),("--bad-option",),("invalid_context",)):
            s.case("runcon",list(args))
    else:
        print("[SKIP] runcon GNU oracle unavailable on this host (SELinux not built)")
    s.product_version("runcon")

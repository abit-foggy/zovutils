"""GNU parity cases for whoami."""
from support import Suite

def run(s: Suite) -> None:
    s.case("whoami", [])
    s.case("whoami", ["--bad-option"])
    s.product_version("whoami")
    s.symlink_dispatch("whoami", [])

"""GNU parity cases for users."""
from support import Suite

def run(s: Suite) -> None:
    s.case("users", [])
    s.product_version("users")
    s.symlink_dispatch("users", [])

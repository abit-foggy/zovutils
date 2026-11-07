"""GNU parity cases for runcon."""
from support import Suite

def run(s: Suite) -> None:
    for args in (("invalid_context","/bin/true"),("--bad-option",),("invalid_context",)):
        s.case("runcon",list(args))
    s.product_version("runcon")
